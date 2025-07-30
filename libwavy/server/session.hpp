#pragma once
/********************************************************************************
 *                                Wavy Project                                  *
 *                         High-Fidelity Audio Streaming                        *
 *                                                                              *
 *  Copyright (c) 2025 Oinkognito                                               *
 *  All rights reserved.                                                        *
 *                                                                              *
 *  License:                                                                    *
 *  This software is licensed under the BSD-3-Clause License. You may use,      *
 *  modify, and distribute this software under the conditions stated in the     *
 *  LICENSE file provided in the project root.                                  *
 *                                                                              *
 *  Warranty Disclaimer:                                                        *
 *  This software is provided "AS IS", without any warranties or guarantees,    *
 *  either expressed or implied, including but not limited to fitness for a     *
 *  particular purpose.                                                         *
 *                                                                              *
 *  Contributions:                                                              *
 *  Contributions are welcome. By submitting code, you agree to license your    *
 *  contributions under the same BSD-3-Clause terms.                            *
 *                                                                              *
 *  See LICENSE file for full legal details.                                    *
 ********************************************************************************/

#include <boost/filesystem.hpp>
#include <boost/iostreams/copy.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/filtering_streambuf.hpp>
#include <boost/thread.hpp>
#include <boost/uuid/random_generator.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <fstream>
#include <sstream>

#include <libwavy/log-macros.hpp>
#include <libwavy/server/methods/download.hpp>
#include <libwavy/toml/toml_parser.hpp>
#include <libwavy/unix/domainBind.hpp>
#include <libwavy/utils/math/entry.hpp>
#include <libwavy/zstd/decompression.h>

namespace bfs = boost::filesystem; // Refers to boost filesystem (async file I/O ops)

using Server       = libwavy::log::SERVER;
using ServerUpload = libwavy::log::SERVER_UPLD;

namespace libwavy::server
{

class WavySession : public std::enable_shared_from_this<WavySession>
{
public:
  explicit WavySession(Socket socket, const IPAddr ip)
      : m_socket(std::move(socket)), m_ipID(std::move(ip))
  {
  }

  void start()
  {
    log::INFO<Server>(LogMode::Async, "Starting new session...");
    do_handshake();
  }

private:
  Socket                           m_socket;
  beast::flat_buffer               m_buffer;
  http::request<http::string_body> m_request;
  IPAddr                           m_ipID;

  void do_handshake()
  {
    auto self(shared_from_this());
    m_socket.async_handshake(boost::asio::ssl::stream_base::server,
                             [this, self](boost::system::error_code ec)
                             {
                               if (ec)
                               {
                                 log::ERROR<Server>(LogMode::Async, "SSL handshake failed: {}",
                                                    ec.message());
                                 return;
                               }
                               log::INFO<Server>(LogMode::Async, "SSL handshake successful!");
                               doRead();
                             });
  }

  void resolveIP()
  {
    try
    {
      m_ipID = m_socket.lowest_layer().remote_endpoint().address().to_string();
      log::INFO<Server>(LogMode::Async, "Resolved IP: {}", m_ipID);
    }
    catch (const std::exception& e)
    {
      log::ERROR<Server>(LogMode::Async, "Failed to resolve IP: {}", e.what());
      SendResponse(macros::to_string(macros::SERVER_ERROR_500));
      return;
    }

    doRead();
  }

  void doRead()
  {
    auto self(shared_from_this());

    auto parser = std::make_shared<http::request_parser<http::string_body>>();
    parser->body_limit(
      WAVY_SERVER_AUDIO_SIZE_LIMIT *
      static_cast<std::size_t>(ONE_MIB)); // for now 200MiB is alright, when lossless codecs come
                                          // in the picture we will have to think about it.

    http::async_read(
      m_socket, m_buffer, *parser,
      [this, self, parser](boost::system::error_code ec, std::size_t bytes_transferred)
      {
        if (ec)
        {
          log::ERROR<Server>(LogMode::Async, "Read error: {}", ec.message());
          if (ec == http::error::body_limit)
          {
            log::ERROR<Server>(LogMode::Async, "Upload size exceeded the limit!");
            SendResponse(macros::to_string(macros::SERVER_ERROR_413));
          }
          return;
        }
        /* ZSTD_bytes_to_mib is a C FFI from common.h */
        log::INFO<Server>(LogMode::Async, "Received {} MiB ({} bytes)",
                          ZSTD_bytes_to_mib(bytes_transferred), bytes_transferred);
        m_request = parser->release();
        ProcessRequest();
      });

    // Timeout to prevent indefinite read
    boost::asio::socket_base::keep_alive option(true);
    m_socket.next_layer().set_option(option);
  }

  auto fetch_metadata(const AbsPath& metadata_path, std::ostringstream& response_stream,
                      const StorageAudioID& audio_id) -> bool
  {
    log::TRACE<Server>(LogMode::Async, "Processing file: {}", metadata_path);

    try
    {
      AudioMetadata metadata = parseAudioMetadata(metadata_path);
      log::DBG<Server>(LogMode::Async, "Successfully parsed metadata for Audio-ID: {}", audio_id);

      auto ResponseWrite = [&](int index, const std::string& label, const std::string& value)
      { response_stream << "      " << index << ". " << label << ": " << value << "\n"; };

      response_stream << "  - " << audio_id << "\n";
      ResponseWrite(1, "Title", metadata.title);
      ResponseWrite(2, "Artist", metadata.artist);
      ResponseWrite(3, "Duration", std::to_string(metadata.duration) + " secs");
      ResponseWrite(4, "Album", metadata.album);
      ResponseWrite(5, "Bitrate", std::to_string(metadata.bitrate) + " kbps");
      ResponseWrite(6, "Sample Rate", std::to_string(metadata.audio_stream.sample_rate) + " Hz");
      ResponseWrite(7, "Sample Format", metadata.audio_stream.sample_format);
      ResponseWrite(8, "Audio Bitrate", std::to_string(metadata.audio_stream.bitrate) + " kbps");
      ResponseWrite(9, "Codec", metadata.audio_stream.codec);

      response_stream << "      10. Available Bitrates: [";
      std::ranges::for_each(metadata.bitrates, [&](auto br) { response_stream << br << ","; });
      response_stream << "]\n";
    }
    catch (const std::exception& e)
    {
      log::ERROR<Server>(LogMode::Async, "Error parsing metadata for Audio-ID {}: {}", audio_id,
                         e.what());
      return false;
    }

    return true;
  }

  // Handle listing the audio metadata request.
  void HandleAMLRequest()
  {
    log::INFO<Server>(LogMode::Async, "Handling Audio Metadata Listing request (AMLR)");

    AbsPath storage_path = macros::to_string(macros::SERVER_STORAGE_DIR);
    if (!bfs::exists(storage_path) || !bfs::is_directory(storage_path))
    {
      log::ERROR<Server>(LogMode::Async, "Storage directory not found: {}", storage_path);
      SendResponse(macros::to_string(macros::SERVER_ERROR_500));
      return;
    }

    std::ostringstream response_stream;
    bool               entries_found = false;

    for (const bfs::directory_entry& nickname_entry : bfs::directory_iterator(storage_path))
    {
      if (bfs::is_directory(nickname_entry.status()))
      {
        const StorageOwnerID nickname = nickname_entry.path().filename().string();
        log::DBG<Server>(LogMode::Async, "Processing Owner Nickname: {}", nickname);
        response_stream << nickname << ":\n";

        bool audio_found = false;
        for (const bfs::directory_entry& audio_entry :
             bfs::directory_iterator(nickname_entry.path()))
        {
          if (bfs::is_directory(audio_entry.status()))
          {
            StorageAudioID audio_id = audio_entry.path().filename().string();
            log::TRACE<Server>(LogMode::Async, "Found Audio-ID: {}", audio_id);

            RelPath metadata_path =
              audio_entry.path().string() + "/" + macros::to_cstr(macros::METADATA_FILE);
            if (bfs::exists(metadata_path))
            {
              log::TRACE<Server>(LogMode::Async, "Found metadata file: {}", metadata_path);
              if (fetch_metadata(metadata_path, response_stream, audio_id))
                audio_found = true;
            }
            else
            {
              log::WARN<Server>(LogMode::Async, "No metadata file found for Audio-ID: {}",
                                audio_id);
            }
          }
        }

        if (!audio_found)
        {
          log::WARN<Server>(LogMode::Async,
                            "No metadata found for any Audio IDs under nickname: {}", nickname);
          response_stream << "  (No metadata found for any audio IDs)\n";
        }

        entries_found = true;
      }
    }

    if (!entries_found)
    {
      log::ERROR<Server>(LogMode::Async,
                         "No nicknames or Audio-IDs with metadata found in storage!");
      SendResponse(macros::to_string(macros::SERVER_ERROR_404));
      return;
    }

    // Return the hierarchical list of IP-IDs, Audio-IDs, and metadata
    SendResponse("HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\n" + response_stream.str());
  }

  // will list all owners' nicknames available in SERVER_STORAGE_DIR.
  void HandleNLRequest()
  {
    log::INFO<Server>(LogMode::Async, "Handling Nicknames Listing Request (NLR)");

    AbsPath storage_path = macros::to_string(macros::SERVER_STORAGE_DIR);
    if (!bfs::exists(storage_path) || !bfs::is_directory(storage_path))
    {
      log::ERROR<Server>(LogMode::Async, "Storage directory not found: {}", storage_path);
      SendResponse(macros::to_string(macros::SERVER_ERROR_500));
      return;
    }

    std::ostringstream response_stream;
    bool               entries_found = false;

    for (const bfs::directory_entry& nickname_entry : bfs::directory_iterator(storage_path))
    {
      if (bfs::is_directory(nickname_entry.status()))
      {
        StorageOwnerID nickname = nickname_entry.path().filename().string();
        response_stream << nickname << ":\n"; // nickname chosen by the owner

        bool audio_found = false;
        for (const bfs::directory_entry& audio_entry :
             bfs::directory_iterator(nickname_entry.path()))
        {
          if (bfs::is_directory(audio_entry.status()))
          {
            response_stream << "  - " << audio_entry.path().filename().string() << "\n"; // Audio-ID
            audio_found = true;
          }
        }

        if (!audio_found)
        {
          response_stream << "  (No audio IDs found)\n";
        }

        entries_found = true;
      }
    }

    if (!entries_found)
    {
      log::ERROR<Server>(LogMode::Async, "No IPs or Audio-IDs found in storage!!");
      SendResponse(macros::to_string(macros::SERVER_ERROR_404));
      return;
    }

    // Return the list of Nicknames and their respective Audio-IDs
    SendResponse("HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\n" + response_stream.str());
  }

  void ProcessRequest()
  {
    if (m_request.method() == http::verb::post)
    {
      // Check if this is a TOML file upload request.
      // For example, if the request target is "/toml/upload"...
      NetTarget target(m_request.target().begin(), m_request.target().end());
      if (target == macros::SERVER_PATH_TOML_UPLOAD)
      {
        // Remove padding text before parsing
        std::string body = m_request.body();
        helpers::removeBodyPadding(body);

        // Call your TOML parsing function.
        auto toml_data_opt = parseAudioMetadataFromDataString(body);
        if (toml_data_opt.path.empty())
        {
          log::ERROR<Server>(LogMode::Async, "[TOML] Failed to parse TOML data");
          SendResponse(macros::to_string(macros::SERVER_ERROR_400));
          return;
        }

        SendResponse("HTTP/1.1 200 OK\r\n\r\nTOML parsed\r\n");
        return;
      }
      HandleUpload();
    }
    else if (m_request.method() == http::verb::get)
    {
      if (m_request.target() == macros::SERVER_PATH_HLS_OWNERS) // Request for all client IDs
      {
        HandleNLRequest();
      }
      else if (m_request.target() == macros::SERVER_PATH_AUDIO_INFO)
      {
        HandleAMLRequest();
      }
      else if (m_request.target() == macros::SERVER_PATH_PING)
      {
        HandleSendPong();
      }
      else
      {
        HandleDownload();
      }
    }
    else
    {
      SendResponse(macros::to_string(macros::SERVER_ERROR_405));
    }
  }

  void HandleSendPong()
  {
    log::INFO<Server>(LogMode::Async, "Sending pong to client...");
    SendResponse(macros::to_string(macros::SERVER_PONG_MSG));
  }

  void HandleUpload()
  {
    // At this point we do not know the owner nor have the intention to know so we identify by
    // a remote IP address
    //
    // [NOTE]
    // When using VPN like tailscale to funnel to connect with other devices over VPN in general,
    // tailscale or any VPN service will not expose your true IP Address; rather it will identify
    // you as the default gateway ip addr for routers (ex: 10.0.0.1)
    //
    // I have tried this with my tailscale and all connections are identified as this IP only!
    //
    // This is majorly the reason why I have moved over owner identification to a "nickname" set
    // by the owner themselves before dispatching it over to the server. The onus now lies on the
    // owner to make it unique and suitable to their needs (they can always check /hls/owners path
    // for currently recognized owners in the server)
    log::INFO<ServerUpload>(LogMode::Async, "Handling GZIP file upload from remote IP: {}", m_ipID);

    const StorageAudioID audio_id = boost::uuids::to_string(boost::uuids::random_generator()());
    const AbsPath gzip_path = macros::to_string(macros::SERVER_TEMP_STORAGE_DIR) + "/" + audio_id +
                              macros::to_string(macros::COMPRESSED_ARCHIVE_EXT);

    bfs::create_directories(macros::SERVER_TEMP_STORAGE_DIR);
    std::ofstream output_file(gzip_path, std::ios::binary);
    if (!output_file)
    {
      log::ERROR<ServerUpload>(LogMode::Async, "Failed to open output file for writing: {}",
                               gzip_path);
      SendResponse(macros::to_string(macros::SERVER_ERROR_500));
      return;
    }

    output_file.write(m_request.body().data(), m_request.body().size());

    if (!output_file.good())
    {
      log::ERROR<ServerUpload>(LogMode::Async, "Failed to write data to file: {}", gzip_path);
      SendResponse(macros::to_string(macros::SERVER_ERROR_500));
      return;
    }

    output_file.close();

    if (!bfs::exists(gzip_path) || bfs::file_size(gzip_path) == 0)
    {
      log::ERROR<ServerUpload>(LogMode::Async, "GZIP upload failed: File is empty or missing!");
      SendResponse("HTTP/1.1 400 Bad Request\r\n\r\nGZIP upload failed");
      return;
    }

    log::INFO<ServerUpload>(LogMode::Async, "File successfully written: {}", gzip_path);

    if (helpers::extract_and_validate(gzip_path, audio_id))
    {
      SendResponse("HTTP/1.1 200 OK\r\nAudio-ID: " + audio_id + "\r\n\r\n");
    }
    else
    {
      log::ERROR<ServerUpload>(LogMode::Async, "Extraction or validation failed!");
      SendResponse(macros::to_string(macros::SERVER_ERROR_400));
    }

    bfs::remove(gzip_path);
  }

  /*
   * NOTE:
   *
   * Currently this is a **NOT PERSISTENT** download approach.
   *
   * The storage model will undergo a revamp but essentially,
   * WavyServer stores verified files (.ts, .m3u8, .m4s, .toml, etc)
   * in `/tmp/wavy_storage`.
   *
   * In Linux, the /tmp is NOT a persistant directory and after every
   * reboot of the system, the /tmp directory is re-created for that
   * session only.
   *
   * The revamp will hope to bring a persistant option as well to fully
   * utilize the server's potential as a audio media server.
   *
   */
  void HandleDownload()
  {
    auto responder =
      std::make_shared<methods::DownloadManager>(std::move(m_socket), std::move(m_request), m_ipID);
    responder->Run();
  }

  void SendResponse(const NetResponse& msg)
  {
    log::DBG<Server>(LogMode::Async, "Attempting to send \n{}", msg);

    auto self(shared_from_this());
    boost::asio::async_write(
      m_socket, boost::asio::buffer(msg),
      [this, self, msg_size = msg.size()](boost::system::error_code ec,
                                          std::size_t               bytes_transferred)
      {
        // Always perform shutdown, even on error
        auto SocketShutdown = [this, self]()
        {
          m_socket.async_shutdown(
            [this, self](boost::system::error_code shutdown_ec)
            {
              if (shutdown_ec)
              {
                log::ERROR<Server>(LogMode::Async, "Shutdown error: {}", shutdown_ec.message());
              }
              m_socket.lowest_layer().close();
            });
        };

        if (ec)
        {
          log::ERROR<Server>(LogMode::Async, "Write error: {}", ec.message());
          SocketShutdown();
          return;
        }

        if (bytes_transferred != msg_size)
        {
          log::ERROR<Server>(LogMode::Async, "Incomplete write: {} of {} bytes.", bytes_transferred,
                             msg_size);
          SocketShutdown();
          return;
        }

        SocketShutdown();
      });
  }
};

} // namespace libwavy::server
