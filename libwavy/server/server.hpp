#pragma once
/************************************************
 * Wavy Project - High-Fidelity Audio Streaming
 * ---------------------------------------------
 * 
 * Copyright (c) 2025 Oinkognito
 * All rights reserved.
 * 
 * This source code is part of the Wavy project, an advanced
 * local networking solution for high-quality audio streaming.
 * 
 * License:
 * This software is licensed under the BSD-3-Clause License.
 * You may use, modify, and distribute this software under
 * the conditions stated in the LICENSE file provided in the
 * project root.
 * 
 * Warranty Disclaimer:
 * This software is provided "AS IS," without any warranties
 * or guarantees, either expressed or implied, including but
 * not limited to fitness for a particular purpose.
 * 
 * Contributions:
 * Contributions to this project are welcome. By submitting 
 * code, you agree to license your contributions under the 
 * same BSD-3-Clause terms.
 * 
 * See LICENSE file for full details.
 ************************************************/

#include <archive.h>
#include <archive_entry.h>
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
#include <libwavy/common/macros.hpp>
#include <sstream>
#include <unistd.h>
#include <vector>

#include <libwavy/toml/toml_parser.hpp>
#include <libwavy/unix/domainBind.hpp>
#include <libwavy/zstd/decompression.h>

/*
 * @SERVER
 *
 * The server's main responsibility the way we see it is:
 *
 * -> Accepting requests through secure SSL transfer with valid certification (TBD) to upload files
 * (POST)
 * -> Upload of GZip file (which supposedly contains .m3u8 and .ts files with apt references and
 * hierarchial format as expected of HLS encoding)
 * -> Validation of the above GZip file's contents to match expected encoded files
 * -> Assign client a UUID upon validation
 * -> Create and expose the client's uploaded content through UUID
 * -> Serve required files to receiver through GET
 *
 *  Server HLS Storage Organization:
 *
 *  hls_storage/
 *  ├── 192.168.1.10/                                    # AUDIO OWNER IP Address 192.168.1.10 (example)
 *  │   ├── 1435f431-a69a-4027-8661-44c31cd11ef6/        # Randomly generated audio id
 *  │   │   ├── index.m3u8
 *  │   │   ├── hls_mp3_64.m3u8                          # HLS MP3 encoded playlist (64-bit)
 *  │   │   ├── hls_mp3_64_0.ts                          # First transport stream of hls_mp3_64 playlist                
 *  │   │   ├── ...                                      # Similarly for 128 and 256 bitrates
 *  │   │   ├── metadata.toml                            # Metadata and other song information
 *  │   ├── e5fdeca5-57c8-47b4-b9c6-60492ddf11ae/
 *  │   │   ├── index.m3u8
 *  │   │   ├── hls_flac_64.m3u8                         # HLS FLAC encoded playlist (64-bit)
 *  │   │   ├── hls_flac_64_0.ts                         # First transport stream of hls_mp3_64 playlist 
 *  │   │   ├── ...                                      # Similarly for 128 and 256 bitrates
 *  │   │   ├── metadata.toml                            # Metadata and other song information
 *  │    
 *
 * Boost libs ensures that every operation (if not then most) in the server occurs asynchronously
 * without any concurrency issues
 *
 * Although not tried and tested with multiple clients, it should give expected results.
 *
 * Boost should also ensure safety and shared lifetimes of a lot of critical objects.
 *
 * @NOTE:
 *
 * ==> This file **CANNOT** be compiled for < -std=c++20
 *
 * Look at Makefile for more information
 *
 * ==> macros::SERVER_STORAGE_DIR and macros::SERVER_TEMP_STORAGE_DIR) are in the same parent
 * directory as boost has problems renaming content to different directories (gives a very big
 * cross-link error)
 *
 */

namespace bfs   = boost::filesystem; // Refers to boost filesystem (async file I/O ops)
namespace beast = boost::beast;
namespace http  = beast::http;
using boost::asio::ip::tcp;

#define ARCHIVE_READ_BUFFER_SIZE 10240

/* PROTOTYPES DEFINITION FOR VALIDATION IN SERVER */
auto is_valid_extension(const std::string& filename) -> bool;
auto validate_m3u8_format(const std::string& content) -> bool;
auto validate_ts_file(const std::vector<uint8_t>& data) -> bool;
auto validate_m4s(const std::string& m4s_path) -> bool;
auto extract_and_validate(const std::string& gzip_path, const std::string& audio_id,
                          const std::string& ip_id) -> bool;
void removeBodyPadding(std::string& body);
auto tokenizePath(std::istringstream& iss) -> vector<std::string>;

namespace libwavy::server
{

class HLS_Session : public std::enable_shared_from_this<HLS_Session>
{
public:
  explicit HLS_Session(boost::asio::ssl::stream<tcp::socket> socket, const std::string ip)
      : socket_(std::move(socket)), ip_id_(std::move(ip))
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
  http::request<http::string_body>      request_;
  std::string                           ip_id_;

  void do_handshake()
  {
    auto self(shared_from_this());
    socket_.async_handshake(boost::asio::ssl::stream_base::server,
                            [this, self](boost::system::error_code ec)
                            {
                              if (ec)
                              {
                                LOG_ERROR << SERVER_LOG << "SSL handshake failed: " << ec.message();
                                return;
                              }
                              LOG_INFO << SERVER_LOG << "SSL handshake successful";
                              do_read();
                            });
  }

  void resolve_ip()
  {
    try
    {
      ip_id_ = socket_.lowest_layer().remote_endpoint().address().to_string();
      LOG_INFO_ASYNC << SERVER_LOG << "Resolved IP: " << ip_id_;
    }
    catch (const std::exception& e)
    {
      LOG_ERROR_ASYNC << SERVER_LOG << "Failed to resolve IP: " << e.what();
      send_response(macros::to_string(macros::SERVER_ERROR_500));
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
            send_response(macros::to_string(macros::SERVER_ERROR_413));
          }
          return;
        }
        /* bytes_to_mib is a C FFI from common.h */
        LOG_INFO_ASYNC << SERVER_LOG << "Received " << bytes_to_mib(bytes_transferred) << " MiB ("
                       << bytes_transferred << ") bytes";
        request_ = parser->release();
        process_request();
      });

    // Timeout to prevent indefinite read
    boost::asio::socket_base::keep_alive option(true);
    socket_.next_layer().set_option(option);
  }

  auto fetch_metadata(const std::string& metadata_path, std::ostringstream& response_stream,
                      const std::string& audio_id) -> bool
  {
    LOG_DEBUG << "[Fetch Metadata] Processing file: " << metadata_path;

    try
    {
      AudioMetadata metadata = parseAudioMetadata(metadata_path);
      LOG_DEBUG_ASYNC << "[Fetch Metadata] Successfully parsed metadata for Audio-ID: " << audio_id;

      response_stream << "  - " << audio_id << "\n";
      response_stream << "      1. Title:              " << metadata.title << "\n";
      response_stream << "      2. Artist:             " << metadata.artist << "\n";
      response_stream << "      3. Album:              " << metadata.album << "\n";
      response_stream << "      4. Bitrate:            " << metadata.bitrate << " kbps\n";
      response_stream << "      5. Audio Bitrate:      " << metadata.audio_stream.bitrate
                      << " kbps\n";
      response_stream << "      6. Codec:              " << metadata.audio_stream.codec << "\n";
      response_stream << "      7. Available Bitrates: [";
      for (const auto& br : metadata.bitrates)
      {
        response_stream << br << ",";
      }
      response_stream << "]\n";
    }
    catch (const std::exception& e)
    {
      LOG_ERROR_ASYNC << "[Fetch Metadata] Error parsing metadata for Audio-ID " << audio_id << ": "
                      << e.what();
      return false;
    }

    return true;
  }

  void handle_list_audio_info()
  {
    LOG_INFO_ASYNC << "[List Audio Info] Handling audio metadata listing request";

    std::string storage_path = macros::to_string(macros::SERVER_STORAGE_DIR);
    if (!bfs::exists(storage_path) || !bfs::is_directory(storage_path))
    {
      LOG_ERROR_ASYNC << "[List Audio Info] Storage directory not found: " << storage_path;
      send_response(macros::to_string(macros::SERVER_ERROR_500));
      return;
    }

    std::ostringstream response_stream;
    bool               entries_found = false;

    for (const bfs::directory_entry& ip_entry : bfs::directory_iterator(storage_path))
    {
      if (bfs::is_directory(ip_entry.status()))
      {
        std::string ip_id = ip_entry.path().filename().string();
        LOG_DEBUG_ASYNC << "[List Audio Info] Processing IP-ID: " << ip_id;
        response_stream << ip_id << ":\n";

        bool audio_found = false;
        for (const bfs::directory_entry& audio_entry : bfs::directory_iterator(ip_entry.path()))
        {
          if (bfs::is_directory(audio_entry.status()))
          {
            std::string audio_id = audio_entry.path().filename().string();
            LOG_DEBUG_ASYNC << "[List Audio Info] Found Audio-ID: " << audio_id;

            std::string metadata_path =
              audio_entry.path().string() + "/" + macros::to_cstr(macros::METADATA_FILE);
            if (bfs::exists(metadata_path))
            {
              LOG_DEBUG_ASYNC << "[List Audio Info] Found metadata file: " << metadata_path;
              if (fetch_metadata(metadata_path, response_stream, audio_id))
                audio_found = true;
            }
            else
            {
              LOG_DEBUG_ASYNC << "[List Audio Info] No metadata file found for Audio-ID: "
                              << audio_id;
            }
          }
        }

        if (!audio_found)
        {
          LOG_WARNING_ASYNC << "[List Audio Info] No metadata found for any audio IDs under IP-ID: "
                            << ip_id;
          response_stream << "  (No metadata found for any audio IDs)\n";
        }

        entries_found = true;
      }
    }

    if (!entries_found)
    {
      LOG_WARNING_ASYNC << "[List Audio Info] No IPs or Audio-IDs with metadata found in storage";
      send_response(macros::to_string(macros::SERVER_ERROR_404));
      return;
    }

    // Return the hierarchical list of IP-IDs, Audio-IDs, and metadata
    send_response("HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\n" + response_stream.str());
  }

  void handle_list_ips()
  {
    LOG_INFO_ASYNC << "[List IPs] Handling IP listing request";

    std::string storage_path = macros::to_string(macros::SERVER_STORAGE_DIR);
    if (!bfs::exists(storage_path) || !bfs::is_directory(storage_path))
    {
      LOG_ERROR_ASYNC << "[List IPs] Storage directory not found: " << storage_path;
      send_response(macros::to_string(macros::SERVER_ERROR_500));
      return;
    }

    std::ostringstream response_stream;
    bool               entries_found = false;

    for (const bfs::directory_entry& ip_entry : bfs::directory_iterator(storage_path))
    {
      if (bfs::is_directory(ip_entry.status()))
      {
        std::string ip_id = ip_entry.path().filename().string();
        response_stream << ip_id << ":\n"; // IP-ID Header

        bool audio_found = false;
        for (const bfs::directory_entry& audio_entry : bfs::directory_iterator(ip_entry.path()))
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
      LOG_WARNING_ASYNC << "[List IPs] No IPs or Audio-IDs found in storage";
      send_response(macros::to_string(macros::SERVER_ERROR_404));
      return;
    }

    // Return the list of IP-IDs and their respective Audio-IDs
    send_response("HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\n" + response_stream.str());
  }

  void process_request()
  {
    if (request_.method() == http::verb::post)
    {
      // Check if this is a TOML file upload request.
      // For example, if the request target is "/toml/upload"...
      std::string target(request_.target().begin(), request_.target().end());
      if (target == macros::SERVER_PATH_TOML_UPLOAD)
      {
        // Remove padding text before parsing
        std::string body = request_.body();
        removeBodyPadding(body);

        // Call your TOML parsing function.
        auto toml_data_opt = parseAudioMetadataFromDataString(body);
        if (toml_data_opt.path.empty())
        {
          LOG_ERROR_ASYNC << "[TOML] Failed to parse TOML data";
          send_response(macros::to_string(macros::SERVER_ERROR_400));
          return;
        }

        send_response("HTTP/1.1 200 OK\r\n\r\nTOML parsed\r\n");
        return;
      }
      handle_upload();
    }
    else if (request_.method() == http::verb::get)
    {
      if (request_.target() == macros::SERVER_PATH_HLS_CLIENTS) // Request for all client IDs
      {
        handle_list_ips();
      }
      else if (request_.target() == macros::SERVER_PATH_AUDIO_INFO)
      {
        handle_list_audio_info();
      }
      else if (request_.target() == macros::SERVER_PATH_PING)
      {
        handle_send_pong();
      }
      else
      {
        handle_download();
      }
    }
    else
    {
      send_response(macros::to_string(macros::SERVER_ERROR_405));
    }
  }

  void handle_send_pong()
  {
    LOG_INFO_ASYNC << "Sending pong to client...";
    send_response(macros::to_string(macros::SERVER_PONG_MSG));
  }

  void handle_upload()
  {
    LOG_INFO_ASYNC << SERVER_UPLD_LOG << "Handling GZIP file upload";

    std::string audio_id  = boost::uuids::to_string(boost::uuids::random_generator()());
    std::string gzip_path = macros::to_string(macros::SERVER_TEMP_STORAGE_DIR) + "/" + audio_id +
                            macros::to_string(macros::COMPRESSED_ARCHIVE_EXT);

    bfs::create_directories(macros::SERVER_TEMP_STORAGE_DIR);
    std::ofstream output_file(gzip_path, std::ios::binary);
    if (!output_file)
    {
      LOG_ERROR_ASYNC << SERVER_UPLD_LOG << "Failed to open output file for writing: " << gzip_path;
      send_response(macros::to_string(macros::SERVER_ERROR_500));
      return;
    }

    output_file.write(request_.body().data(), request_.body().size());

    if (!output_file.good())
    {
      LOG_ERROR_ASYNC << SERVER_UPLD_LOG << "Failed to write data to file: " << gzip_path;
      send_response(macros::to_string(macros::SERVER_ERROR_500));
      return;
    }

    output_file.close();

    if (!bfs::exists(gzip_path) || bfs::file_size(gzip_path) == 0)
    {
      LOG_ERROR_ASYNC << SERVER_UPLD_LOG << "GZIP upload failed: File is empty or missing!";
      send_response("HTTP/1.1 400 Bad Request\r\n\r\nGZIP upload failed");
      return;
    }

    LOG_INFO_ASYNC << SERVER_UPLD_LOG << "File successfully written: " << gzip_path;

    if (extract_and_validate(gzip_path, audio_id, ip_id_))
    {
      send_response("HTTP/1.1 200 OK\r\nClient-ID: " + audio_id + "\r\n\r\n");
    }
    else
    {
      LOG_ERROR_ASYNC << SERVER_UPLD_LOG << "Extraction or validation failed!";
      send_response(macros::to_string(macros::SERVER_ERROR_400));
    }

    bfs::remove(gzip_path);
  }

  /*
   * NOTE:
   *
   * Currently this is a **PERSISTENT** download approach.
   *
   * So previous clients storage is also present regardless of whether the server was restarted or
   * not.
   *
   * This means that unless the server's filesystem purposefully removes the hls storage, the
   * client's extracted data will *ALWAYS* be present in the server.
   *
   * TLDR: Previous server instances of client storage is persistent and can be accessed until they
   * are removed from server's filesystem.
   *
   */
  void handle_download()
  {
    // Parse request target (expected: /hls/<audio_id>/<filename>)
    std::string        target(request_.target().begin(), request_.target().end());
    std::istringstream iss(target);

    LOG_DEBUG_ASYNC << SERVER_DWNLD_LOG << " Processing request: " << target;

    std::vector<std::string> parts = tokenizePath(iss);

    if (parts.size() < 4 || parts[0] != "hls")
    {
      LOG_ERROR_ASYNC << SERVER_DWNLD_LOG << " Invalid request path: " << target;
      send_response(macros::to_string(macros::SERVER_ERROR_400));
      return;
    }

    std::string ip_addr  = parts[1];
    std::string audio_id = parts[2];
    std::string filename = parts[3];

    // Construct the file path
    std::string file_path = macros::to_string(macros::SERVER_STORAGE_DIR) + "/" + ip_addr + "/" +
                            audio_id + "/" + filename;

    LOG_DEBUG_ASYNC << SERVER_DWNLD_LOG << " Checking file: " << file_path;

    // Use async file reading (prevent blocking on large files)
    auto file_stream = std::make_shared<boost::beast::http::file_body::value_type>();

    boost::system::error_code ec;
    file_stream->open(file_path.c_str(), boost::beast::file_mode::read, ec);

    if (ec)
    {
      LOG_ERROR_ASYNC << SERVER_DWNLD_LOG << " Failed to open file: " << file_path
                      << " Error: " << ec.message();
      send_response(macros::to_string(macros::SERVER_ERROR_500));
      return;
    }

    std::uint64_t file_size = file_stream->size();
    LOG_INFO_ASYNC << SERVER_DWNLD_LOG << " File opened asynchronously: " << file_path
                   << " (Size: " << file_size << " bytes)";

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
    response->version(request_.version());
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
                          LOG_ERROR_ASYNC << SERVER_DWNLD_LOG << " Write error: " << ec.message();
                        }
                        else
                        {
                          LOG_INFO_ASYNC << SERVER_DWNLD_LOG << " Response sent successfully.";
                        }
                        socket_.lowest_layer().close();
                      });

    LOG_INFO_ASYNC << SERVER_DWNLD_LOG << " [OWNER:" << ip_addr << "] Served: " << filename << " ("
                   << audio_id << ")";
  }

  void send_response(const std::string& msg)
  {
    LOG_DEBUG << SERVER_LOG << "Attempting to send " << msg;
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

class HLS_Server
{
public:
  HLS_Server(boost::asio::io_context& io_context, boost::asio::ssl::context& ssl_context,
             short port)
      : acceptor_(io_context, tcp::endpoint(tcp::v4(), port)), ssl_context_(ssl_context),
        signals_(io_context, SIGINT, SIGTERM, SIGHUP),
        socketPath(macros::to_string(macros::SERVER_LOCK_FILE)), wavySocketBind(socketPath)
  {
    wavySocketBind.ensure_single_instance();
    LOG_INFO << SERVER_LOG << "Starting HLS server on port " << port;

    signals_.async_wait(
      [this](boost::system::error_code /*ec*/, int /*signo*/)
      {
        LOG_INFO << SERVER_LOG << "Termination signal received. Cleaning up...";
        wavySocketBind.cleanup();
        std::exit(0);
      });

    start_accept();
  }

  ~HLS_Server() { wavySocketBind.cleanup(); }

private:
  tcp::acceptor              acceptor_;
  boost::asio::ssl::context& ssl_context_;
  boost::asio::signal_set    signals_;
  std::string                socketPath;
  unix::UnixSocketBind       wavySocketBind;

  void start_accept()
  {
    acceptor_.async_accept(
      [this](boost::system::error_code ec, tcp::socket socket)
      {
        if (ec)
        {
          LOG_ERROR_ASYNC << SERVER_LOG << "Accept failed: " << ec.message();
          return;
        }

        std::string ip = socket.remote_endpoint().address().to_string();
        LOG_INFO_ASYNC << SERVER_LOG << "Accepted new connection from " << ip;

        auto session = std::make_shared<HLS_Session>(
          boost::asio::ssl::stream<tcp::socket>(std::move(socket), ssl_context_), ip);
        session->start();
        start_accept();
      });
  }
};

} // namespace libwavy::server
