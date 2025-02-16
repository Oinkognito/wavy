#if __cplusplus < 202002L
#error "Wavy-Server requires C++20 or later."
#endif

#include "../include/logger.hpp"
#include "../include/macros.hpp"
#include <archive.h>
#include <archive_entry.h>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/beast.hpp>
#include <boost/filesystem.hpp>
#include <boost/iostreams/copy.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/filtering_streambuf.hpp>
#include <boost/uuid/random_generator.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>

/*
 * SERVER
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
 * ==> STORAGE_DIR and TEMP_DIR are in the same parent directory as boost has problems renaming
 * content to different directories (gives a very big cross-link error)
 *
 */

namespace fs    = boost::filesystem;
namespace beast = boost::beast;
namespace http  = beast::http;
using boost::asio::ip::tcp;

const std::string TEMP_DIR    = "/tmp/hls_temp";
const std::string STORAGE_DIR = "/tmp/hls_storage"; // this will use /tmp of the server's filesystem

auto is_valid_extension(const std::string& filename) -> bool
{
  return filename.ends_with(macros::to_string(macros::PLAYLIST_EXT)) || filename.ends_with(".ts");
}

auto validate_m3u8_format(const std::string& content) -> bool
{
  return content.find("#EXTM3U") != std::string::npos;
}

auto validate_ts_file(const std::vector<uint8_t>& data) -> bool
{
  return !data.empty() && data[0] == 0x47; // MPEG-TS sync byte
}

auto extract_gzip(const std::string& gzip_path, const std::string& extract_path) -> bool
{
  LOG_INFO << "[Extract] Extracting GZIP file: " << gzip_path;

  struct archive*       a   = archive_read_new();
  struct archive*       ext = archive_write_disk_new();
  struct archive_entry* entry;

  archive_read_support_filter_gzip(a);
  archive_read_support_format_tar(a);
  archive_write_disk_set_options(ext, ARCHIVE_EXTRACT_PERM);

  if (archive_read_open_filename(a, gzip_path.c_str(), 10240) != ARCHIVE_OK)
  {
    LOG_ERROR << "[Extract] Failed to open archive: " << archive_error_string(a);
    archive_read_free(a);
    archive_write_free(ext);
    return false;
  }

  bool valid_files_found = false;

  while (archive_read_next_header(a, &entry) == ARCHIVE_OK)
  {
    std::string filename    = archive_entry_pathname(entry);
    std::string output_file = extract_path + "/" + filename;

    LOG_INFO << "[Extract] Extracting file: " << output_file;

    archive_entry_set_pathname(entry, output_file.c_str());

    if (archive_write_header(ext, entry) == ARCHIVE_OK)
    {
      std::ofstream ofs(output_file, std::ios::binary);
      if (!ofs)
      {
        LOG_ERROR << "[Extract] Failed to open file for writing: " << output_file;
        continue;
      }

      char    buffer[8192]; // maybe better off using std::array<>?
      ssize_t len;
      while ((len = archive_read_data(a, buffer, sizeof(buffer))) > 0)
      {
        ofs.write(buffer, len);
      }
      ofs.close();

      valid_files_found = true;
    }
  }

  archive_read_free(a);
  archive_write_free(ext);

  return valid_files_found;
}

auto extract_and_validate(const std::string& gzip_path, const std::string& client_id) -> bool
{
  LOG_INFO << "[Extract] Validating and extracting GZIP file: " << gzip_path;

  if (!fs::exists(gzip_path))
  {
    LOG_ERROR << "[Extract] File does not exist: " << gzip_path;
    return false;
  }

  std::string temp_extract_path = TEMP_DIR + "/" + client_id;
  fs::create_directories(temp_extract_path);

  if (!extract_gzip(gzip_path, temp_extract_path))
  {
    LOG_ERROR << "[Extract] Extraction failed!";
    return false;
  }

  LOG_INFO << "[Extract] Extraction complete, validating files...";

  // Move valid files to storage
  std::string storage_path = STORAGE_DIR + "/" + client_id;
  fs::create_directories(storage_path);

  int valid_file_count = 0;

  for (fs::directory_entry& file : fs::directory_iterator(temp_extract_path))
  {
    std::string          fname = file.path().filename().string();
    std::ifstream        infile(file.path().string(), std::ios::binary);
    std::vector<uint8_t> data((std::istreambuf_iterator<char>(infile)), {});

    if (fname.ends_with(macros::to_string(macros::PLAYLIST_EXT)))
    {
      if (!validate_m3u8_format(std::string(data.begin(), data.end())))
      {
        LOG_WARNING << "[Extract] Invalid M3U8 file, removing: " << fname;
        fs::remove(file.path());
        continue;
      }
    }
    else if (fname.ends_with(".ts"))
    {
      if (!validate_ts_file(data))
      {
        LOG_WARNING << "[Extract] Invalid TS file, removing: " << fname;
        fs::remove(file.path());
        continue;
      }
    }
    else
    {
      LOG_WARNING << "[Extract] Skipping unknown file: " << fname;
      fs::remove(file.path());
      continue;
    }

    // Move validated file to storage
    fs::rename(file.path(), storage_path + "/" + fname);
    LOG_INFO << "[Extract] File stored: " << storage_path + "/" + fname;
    valid_file_count++;
  }

  if (valid_file_count == 0)
  {
    LOG_ERROR << "[Extract] No valid files remain after validation, extraction failed!";
    return false;
  }

  LOG_INFO << "[Extract] Extraction and validation successful.";
  return true;
}

class HLS_Session : public std::enable_shared_from_this<HLS_Session>
{
public:
  explicit HLS_Session(boost::asio::ssl::stream<tcp::socket> socket) : socket_(std::move(socket)) {}

  void start()
  {
    LOG_INFO << "[Session] Starting new session";
    do_handshake();
  }

private:
  boost::asio::ssl::stream<tcp::socket> socket_;
  beast::flat_buffer                    buffer_;
  http::request<http::string_body>      request_;

  void do_handshake()
  {
    auto self(shared_from_this());
    socket_.async_handshake(boost::asio::ssl::stream_base::server,
                            [this, self](boost::system::error_code ec)
                            {
                              if (ec)
                              {
                                LOG_ERROR << "[Session] SSL handshake failed: " << ec.message();
                                return;
                              }
                              LOG_INFO << "[Session] SSL handshake successful";
                              do_read();
                            });
  }

  void do_read()
  {
    auto self(shared_from_this());

    auto parser = std::make_shared<http::request_parser<http::string_body>>();
    parser->body_limit(100 * 1024 * 1024); // for now 100MiB is alright, when lossless codecs come
                                           // in the picture we will have to think about it.

    http::async_read(
      socket_, buffer_, *parser,
      [this, self, parser](boost::system::error_code ec, std::size_t bytes_transferred)
      {
        if (ec)
        {
          LOG_ERROR << "[Session] Read error: " << ec.message();
          if (ec == http::error::body_limit)
          {
            LOG_ERROR << "[Session] Upload size exceeded the limit!";
            send_response("HTTP/1.1 413 Payload Too Large\r\n\r\n");
          }
          return;
        }

        LOG_INFO << "[Session] Received " << bytes_transferred << " bytes";
        request_ = parser->release();
        process_request();
      });

    // Timeout to prevent indefinite read
    boost::asio::socket_base::keep_alive option(true);
    socket_.next_layer().set_option(option);
  }

  void process_request()
  {
    if (request_.method() == http::verb::post)
    {
      handle_upload();
    }
    else if (request_.method() == http::verb::get)
    {
      handle_download();
    }
    else
    {
      send_response("HTTP/1.1 405 Method Not Allowed\r\n\r\n");
    }
  }

  void handle_upload()
  {
    LOG_INFO << "[Upload] Handling GZIP file upload";

    std::string client_id = boost::uuids::to_string(boost::uuids::random_generator()());
    std::string gzip_path = TEMP_DIR + "/" + client_id + macros::to_string(macros::COMPRESSED_ARCHIVE_EXT);

    fs::create_directories(TEMP_DIR);
    std::ofstream output_file(gzip_path, std::ios::binary);
    if (!output_file)
    {
      LOG_ERROR << "[Upload] Failed to open output file for writing: " << gzip_path;
      send_response("HTTP/1.1 500 Internal Server Error\r\n\r\nFile write error");
      return;
    }

    output_file.write(request_.body().data(), request_.body().size());

    if (!output_file.good())
    {
      LOG_ERROR << "[Upload] Failed to write data to file: " << gzip_path;
      send_response("HTTP/1.1 500 Internal Server Error\r\n\r\nFile write error");
      return;
    }

    output_file.close();

    if (!fs::exists(gzip_path) || fs::file_size(gzip_path) == 0)
    {
      LOG_ERROR << "[Upload] GZIP upload failed: File is empty or missing!";
      send_response("HTTP/1.1 400 Bad Request\r\n\r\nGZIP upload failed");
      return;
    }

    LOG_INFO << "[Upload] File successfully written: " << gzip_path;

    if (extract_and_validate(gzip_path, client_id))
    {
      send_response("HTTP/1.1 200 OK\r\nClient-ID: " + client_id + "\r\n\r\n");
    }
    else
    {
      LOG_ERROR << "[Upload] Extraction or validation failed!";
      send_response("HTTP/1.1 400 Bad Request\r\n\r\nInvalid GZIP file");
    }

    fs::remove(gzip_path);
  }

  void handle_download()
  {
    // Parse request target (expected: /hls/<client_id>/<filename>)
    std::string              target(request_.target().begin(), request_.target().end());
    std::vector<std::string> parts;
    std::istringstream       iss(target);
    std::string              token;

    while (std::getline(iss, token, '/'))
    {
      if (!token.empty())
      {
        parts.push_back(token);
      }
    }

    if (parts.size() < 3 || parts[0] != "hls")
    {
      LOG_ERROR << "[Download] Invalid request path: " << target;
      send_response("HTTP/1.1 400 Bad Request\r\n\r\nInvalid request format");
      return;
    }

    std::string client_id = parts[1];
    std::string filename  = parts[2];

    // Construct the file path
    std::string file_path = STORAGE_DIR + "/" + client_id + "/" + filename;

    if (!fs::exists(file_path) || !fs::is_regular_file(file_path))
    {
      LOG_ERROR << "[Download] File not found: " << file_path;
      send_response("HTTP/1.1 404 Not Found\r\n\r\nFile not found");
      return;
    }

    std::ifstream file(file_path, std::ios::binary);
    if (!file)
    {
      LOG_ERROR << "[Download] Failed to open file: " << file_path;
      send_response("HTTP/1.1 500 Internal Server Error\r\n\r\nUnable to read file");
      return;
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();
    std::string file_content = buffer.str();

    std::string content_type = macros::to_string(macros::CONTENT_TYPE_OCTET_STREAM);
    if (filename.ends_with(macros::to_string(macros::PLAYLIST_EXT)))
    {
      content_type = "application/vnd.apple.mpegurl";
    }
    else if (filename.ends_with(macros::to_string(macros::TRANSPORT_STREAM_EXT)))
    {
      content_type = "video/mp2t";
    }

    // Use a shared_ptr to keep the response alive until async_write completes
    auto response = std::make_shared<http::response<http::string_body>>();
    response->version(request_.version());
    response->result(http::status::ok);
    response->set(http::field::server, "Wavy-Server");
    response->set(http::field::content_type, content_type);
    response->body() = std::move(file_content);
    response->prepare_payload();

    auto self = shared_from_this(); // Keep session alive
    http::async_write(socket_, *response,
                      [this, self, response](boost::system::error_code ec, std::size_t)
                      {
                        if (ec)
                        {
                          LOG_ERROR << "[Download] Write error: " << ec.message();
                        }
                        socket_.lowest_layer().close();
                      });

    LOG_INFO << "[Download] Successfully served file: " << file_path;
  }

  void send_response(const std::string& msg)
  {
    auto self(shared_from_this());
    boost::asio::async_write(socket_, boost::asio::buffer(msg),
                             [this, self](boost::system::error_code ec, std::size_t)
                             {
                               if (ec)
                               {
                                 LOG_ERROR << "[Session] Write error: " << ec.message();
                               }
                               socket_.lowest_layer().close();
                             });
  }
};

class HLS_Server
{
public:
  HLS_Server(boost::asio::io_context& io_context, boost::asio::ssl::context& ssl_context,
             short port)
      : acceptor_(io_context, tcp::endpoint(tcp::v4(), port)), ssl_context_(ssl_context)
  {
    LOG_INFO << "[Server] Starting HLS server on port " << port;
    start_accept();
  }

private:
  tcp::acceptor              acceptor_;
  boost::asio::ssl::context& ssl_context_;

  void start_accept()
  {
    acceptor_.async_accept(
      [this](boost::system::error_code ec, tcp::socket socket)
      {
        if (ec)
        {
          LOG_ERROR << "[Server] Accept failed: " << ec.message();
          return;
        }

        LOG_INFO << "[Server] Accepted new connection";
        auto session = std::make_shared<HLS_Session>(
          boost::asio::ssl::stream<tcp::socket>(std::move(socket), ssl_context_));
        session->start();
        start_accept();
      });
  }
};

auto main() -> int
{
  try
  {
    logger::init_logging();
    boost::asio::io_context   io_context;
    boost::asio::ssl::context ssl_context(boost::asio::ssl::context::sslv23);

    ssl_context.set_options(
      boost::asio::ssl::context::default_workarounds | boost::asio::ssl::context::no_sslv2 |
      boost::asio::ssl::context::no_sslv3 | boost::asio::ssl::context::single_dh_use);

    ssl_context.use_certificate_file("server.crt", boost::asio::ssl::context::pem);
    ssl_context.use_private_key_file("server.key", boost::asio::ssl::context::pem);

    HLS_Server server(io_context, ssl_context, 8080);
    io_context.run();
  }
  catch (std::exception& e)
  {
    LOG_ERROR << "[Main] Exception: " << e.what();
  }

  return 0;
}
