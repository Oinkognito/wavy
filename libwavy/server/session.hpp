#pragma once

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/beast.hpp>
#include <boost/filesystem.hpp>
#include <boost/iostreams/copy.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/filtering_streambuf.hpp>
#include <boost/thread.hpp>
#include <boost/uuid/random_generator.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <fstream>
#include <sstream>

#include <libwavy/common/macros.hpp>
#include <libwavy/common/types.hpp>
#include <libwavy/logger.hpp>
#include <libwavy/server/prototypes.hpp>
#include <libwavy/toml/toml_parser.hpp>
#include <libwavy/unix/domainBind.hpp>
#include <libwavy/utils/math/entry.hpp>
#include <libwavy/zstd/decompression.h>

namespace bfs   = boost::filesystem; // Refers to boost filesystem (async file I/O ops)
namespace beast = boost::beast;
namespace http  = beast::http;
using boost::asio::ip::tcp;

namespace libwavy::server
{

class WavySession : public std::enable_shared_from_this<WavySession>
{
public:
  explicit WavySession(boost::asio::ssl::stream<tcp::socket> socket, const IPAddr ip)
      : socket_(std::move(socket)), m_ipID(std::move(ip))
  {
  }

  void start()
  {
    LOG_INFO_ASYNC << SERVER_LOG << "Starting new session";
    do_handshake();
  }

private:
  boost::asio::ssl::stream<tcp::socket> socket_;
  beast::flat_buffer                    buffer_;
  http::request<http::string_body>      m_request;
  IPAddr                                m_ipID;

  void do_handshake()
  {
    auto self(shared_from_this());
    socket_.async_handshake(boost::asio::ssl::stream_base::server,
                            [this, self](boost::system::error_code ec)
                            {
                              if (ec)
                              {
                                LOG_ERROR_ASYNC << SERVER_LOG
                                                << "SSL handshake failed: " << ec.message();
                                return;
                              }
                              LOG_INFO_ASYNC << SERVER_LOG << "SSL handshake successful";
                              do_read();
                            });
  }

  void resolve_ip()
  {
    try
    {
      m_ipID = socket_.lowest_layer().remote_endpoint().address().to_string();
      LOG_INFO_ASYNC << SERVER_LOG << "Resolved IP: " << m_ipID;
    }
    catch (const std::exception& e)
    {
      LOG_ERROR_ASYNC << SERVER_LOG << "Failed to resolve IP: " << e.what();
      SendResponse(macros::to_string(macros::SERVER_ERROR_500));
      return;
    }

    do_read();
  }

  void do_read()
  {
    auto self(shared_from_this());

    auto parser = std::make_shared<http::request_parser<http::string_body>>();
    parser->body_limit(WAVY_SERVER_AUDIO_SIZE_LIMIT * 1024 *
                       1024); // for now 200MiB is alright, when lossless codecs come
                              // in the picture we will have to think about it.

    http::async_read(
      socket_, buffer_, *parser,
      [this, self, parser](boost::system::error_code ec, std::size_t bytes_transferred)
      {
        if (ec)
        {
          LOG_ERROR_ASYNC << SERVER_LOG << "Read error: " << ec.message();
          if (ec == http::error::body_limit)
          {
            LOG_ERROR_ASYNC << SERVER_LOG << "Upload size exceeded the limit!";
            SendResponse(macros::to_string(macros::SERVER_ERROR_413));
          }
          return;
        }
        /* bytes_to_mib is a C FFI from common.h */
        LOG_INFO_ASYNC << SERVER_LOG << "Received " << ZSTD_bytes_to_mib(bytes_transferred)
                       << " MiB (" << bytes_transferred << ") bytes";
        m_request = parser->release();
        ProcessRequest();
      });

    // Timeout to prevent indefinite read
    boost::asio::socket_base::keep_alive option(true);
    socket_.next_layer().set_option(option);
  }

  auto fetch_metadata(const AbsPath& metadata_path, std::ostringstream& response_stream,
                      const StorageAudioID& audio_id) -> bool
  {
    LOG_TRACE_ASYNC << "Processing file: " << metadata_path;

    try
    {
      AudioMetadata metadata = parseAudioMetadata(metadata_path);
      LOG_DEBUG_ASYNC << "[Fetch Metadata] Successfully parsed metadata for Audio-ID: " << audio_id;

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
      LOG_ERROR_ASYNC << "Error parsing metadata for Audio-ID " << audio_id << ": " << e.what();
      return false;
    }

    return true;
  }

  // Handle listing the audio metadata request.
  void HandleAMLRequest()
  {
    LOG_INFO_ASYNC << "Handling Audio Metadata Listing request (AMLR)";

    std::string storage_path = macros::to_string(macros::SERVER_STORAGE_DIR);
    if (!bfs::exists(storage_path) || !bfs::is_directory(storage_path))
    {
      LOG_ERROR_ASYNC << "Storage directory not found: " << storage_path;
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
        LOG_DEBUG_ASYNC << "Processing nickname: " << nickname;
        response_stream << nickname << ":\n";

        bool audio_found = false;
        for (const bfs::directory_entry& audio_entry :
             bfs::directory_iterator(nickname_entry.path()))
        {
          if (bfs::is_directory(audio_entry.status()))
          {
            StorageAudioID audio_id = audio_entry.path().filename().string();
            LOG_TRACE_ASYNC << "Found Audio-ID: " << audio_id;

            RelPath metadata_path =
              audio_entry.path().string() + "/" + macros::to_cstr(macros::METADATA_FILE);
            if (bfs::exists(metadata_path))
            {
              LOG_TRACE_ASYNC << "Found metadata file: " << metadata_path;
              if (fetch_metadata(metadata_path, response_stream, audio_id))
                audio_found = true;
            }
            else
            {
              LOG_WARNING_ASYNC << "No metadata file found for Audio-ID: " << audio_id;
            }
          }
        }

        if (!audio_found)
        {
          LOG_WARNING_ASYNC << "No metadata found for any audio IDs under nickname: " << nickname;
          response_stream << "  (No metadata found for any audio IDs)\n";
        }

        entries_found = true;
      }
    }

    if (!entries_found)
    {
      LOG_ERROR_ASYNC << "No nicknames or Audio-IDs with metadata found in storage";
      SendResponse(macros::to_string(macros::SERVER_ERROR_404));
      return;
    }

    // Return the hierarchical list of IP-IDs, Audio-IDs, and metadata
    SendResponse("HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\n" + response_stream.str());
  }

  // will list all owners' nicknames available in SERVER_STORAGE_DIR.
  void HandleNLRequest()
  {
    LOG_INFO_ASYNC << "Handling Nicknames Listing Request (NLR)";

    AbsPath storage_path = macros::to_string(macros::SERVER_STORAGE_DIR);
    if (!bfs::exists(storage_path) || !bfs::is_directory(storage_path))
    {
      LOG_ERROR_ASYNC << "Storage directory not found: " << storage_path;
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
      LOG_ERROR_ASYNC << "No IPs or Audio-IDs found in storage";
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
        removeBodyPadding(body);

        // Call your TOML parsing function.
        auto toml_data_opt = parseAudioMetadataFromDataString(body);
        if (toml_data_opt.path.empty())
        {
          LOG_ERROR_ASYNC << "[TOML] Failed to parse TOML data";
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
      if (m_request.target() == macros::SERVER_PATH_HLS_CLIENTS) // Request for all client IDs
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
    LOG_INFO_ASYNC << "Sending pong to client...";
    SendResponse(macros::to_string(macros::SERVER_PONG_MSG));
  }

  void HandleUpload()
  {
    LOG_INFO_ASYNC << SERVER_UPLD_LOG << "Handling GZIP file upload for owner (IP): " << m_ipID;

    StorageAudioID audio_id  = boost::uuids::to_string(boost::uuids::random_generator()());
    AbsPath        gzip_path = macros::to_string(macros::SERVER_TEMP_STORAGE_DIR) + "/" + audio_id +
                        macros::to_string(macros::COMPRESSED_ARCHIVE_EXT);

    bfs::create_directories(macros::SERVER_TEMP_STORAGE_DIR);
    std::ofstream output_file(gzip_path, std::ios::binary);
    if (!output_file)
    {
      LOG_ERROR_ASYNC << SERVER_UPLD_LOG << "Failed to open output file for writing: " << gzip_path;
      SendResponse(macros::to_string(macros::SERVER_ERROR_500));
      return;
    }

    output_file.write(m_request.body().data(), m_request.body().size());

    if (!output_file.good())
    {
      LOG_ERROR_ASYNC << SERVER_UPLD_LOG << "Failed to write data to file: " << gzip_path;
      SendResponse(macros::to_string(macros::SERVER_ERROR_500));
      return;
    }

    output_file.close();

    if (!bfs::exists(gzip_path) || bfs::file_size(gzip_path) == 0)
    {
      LOG_ERROR_ASYNC << SERVER_UPLD_LOG << "GZIP upload failed: File is empty or missing!";
      SendResponse("HTTP/1.1 400 Bad Request\r\n\r\nGZIP upload failed");
      return;
    }

    LOG_INFO_ASYNC << SERVER_UPLD_LOG << "File successfully written: " << gzip_path;

    if (extract_and_validate(gzip_path, audio_id))
    {
      SendResponse("HTTP/1.1 200 OK\r\nClient-ID: " + audio_id + "\r\n\r\n");
    }
    else
    {
      LOG_ERROR_ASYNC << SERVER_UPLD_LOG << "Extraction or validation failed!";
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
    // Parse request target (expected: /hls/<audio_id>/<filename>)
    NetTarget          target(m_request.target().begin(), m_request.target().end());
    std::istringstream iss(target);

    LOG_DEBUG_ASYNC << SERVER_DWNLD_LOG << "Processing request: " << target;

    std::vector<std::string> parts = tokenizePath(iss);

    if (parts.size() < 4 || parts[0] != "hls")
    {
      LOG_ERROR_ASYNC << SERVER_DWNLD_LOG << "Invalid request path: " << target;
      SendResponse(macros::to_string(macros::SERVER_ERROR_400));
      return;
    }

    IPAddr         ip_addr  = parts[1];
    StorageAudioID audio_id = parts[2];
    FileName       filename = parts[3];

    // Construct the file path
    AbsPath file_path = macros::to_string(macros::SERVER_STORAGE_DIR) + "/" + ip_addr + "/" +
                        audio_id + "/" + filename;

    LOG_DEBUG_ASYNC << SERVER_DWNLD_LOG << "Checking file: " << file_path;

    // Use async file reading (prevent blocking on large files)
    auto file_stream = std::make_shared<boost::beast::http::file_body::value_type>();

    boost::system::error_code ec;
    file_stream->open(file_path.c_str(), boost::beast::file_mode::read, ec);

    if (ec)
    {
      LOG_ERROR_ASYNC << SERVER_DWNLD_LOG << "Failed to open file: " << file_path
                      << " Error: " << ec.message();
      SendResponse(macros::to_string(macros::SERVER_ERROR_500));
      return;
    }

    ui8 file_size = file_stream->size();
    LOG_INFO_ASYNC << SERVER_DWNLD_LOG << "File opened asynchronously: " << file_path
                   << " (Size: " << libwavy::utils::math::bytesFormat(file_size) << ")";

    std::string content_type = macros::to_string(macros::CONTENT_TYPE_OCTET_STREAM);
    if (filename.ends_with(macros::PLAYLIST_EXT))
    {
      content_type = "application/vnd.apple.mpegurl";
    }
    else if (filename.ends_with(macros::TRANSPORT_STREAM_EXT))
    {
      content_type = "video/mp2t";
    }

    auto response = std::make_shared<http::response<http::file_body>>();
    response->version(m_request.version());
    response->result(http::status::ok);
    response->set(http::field::server, "Wavy Server");
    response->set(http::field::content_type, content_type);
    response->content_length(file_size);
    response->body() = std::move(*file_stream);
    response->prepare_payload();

    auto self = shared_from_this(); // Keep session alive
    http::async_write(socket_, *response,
                      [this, self, response](boost::system::error_code ec, std::size_t)
                      {
                        if (ec)
                        {
                          LOG_ERROR_ASYNC << SERVER_DWNLD_LOG << "Write error: " << ec.message();
                        }
                        else
                        {
                          LOG_INFO_ASYNC << SERVER_DWNLD_LOG << "Response sent successfully.";
                        }
                        socket_.lowest_layer().close();
                      });

    LOG_INFO_ASYNC << SERVER_DWNLD_LOG << "[OWNER:" << ip_addr << "] Served: " << filename << " ("
                   << audio_id << ")";
  }

  void SendResponse(const NetResponse& msg)
  {
    LOG_DEBUG_ASYNC << SERVER_LOG << "Attempting to send " << msg;
    auto self(shared_from_this());
    boost::asio::async_write(
      socket_, boost::asio::buffer(msg),
      [this, self, msg_size = msg.size()](boost::system::error_code ec,
                                          std::size_t               bytes_transferred)
      {
        // Always perform shutdown, even on error
        auto do_shutdown = [this, self]()
        {
          socket_.async_shutdown(
            [this, self](boost::system::error_code shutdown_ec)
            {
              if (shutdown_ec)
              {
                LOG_ERROR_ASYNC << SERVER_LOG << "Shutdown error: " << shutdown_ec.message();
              }
              socket_.lowest_layer().close();
            });
        };

        if (ec)
        {
          LOG_ERROR_ASYNC << SERVER_LOG << "Write error: " << ec.message();
          do_shutdown();
          return;
        }

        if (bytes_transferred != msg_size)
        {
          LOG_ERROR_ASYNC << SERVER_LOG << "Incomplete write: " << bytes_transferred << " of "
                          << msg_size << " bytes";
          do_shutdown();
          return;
        }

        do_shutdown();
      });
  }
};

} // namespace libwavy::server
