#include <archive.h>
#include <archive_entry.h>
#include <boost/algorithm/string.hpp>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast.hpp>
#include <boost/beast/ssl.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <unordered_map>
#include <vector>

#include "../include/compression.h"
#include "../include/logger.hpp"
#include "../include/macros.hpp"

/*
 * DISPATCHER
 *
 * After encoder encodes the song into HLS streams and stores them in an output directory,
 *
 * The dispatchers job is to find every related playlist file, transport stream, link them and
 * finally compress then into a gzip file (reduces data size to be transferred by sizeable amount)
 *
 * The gzip file is then transferred to the server using boost beast SSL connection
 *
 * NOTE:
 *
 * SSL certification is self signed. So when trying to send a request to the server (using curl for
 * example),
 * -> You need to give the `-k` parameter, as curl cannot certify and validate self-signed certs
 *
 * Maybe we will use certbot or something like that to certify for our domain but since this is in
 * testing this is aight.
 *
 * @FAQ
 *
 * ----------------------------------------------------------------------------------------------------
 *
 * 1. Why not just send the song over and have a codec in the server?
 *
 * Because now you need a codec in the server. In this model of architecture of the application,
 * the aim is to reduce the server load and it's operational reach. It is supposed to be that way,
 * so that it can focus on being a stable and efficient multi-client network gateway.
 *
 * This also works well in case you want a docker image to run as your server, then something
 * minimal like an alpine image with minimal dependencies is always appreciated.
 *
 * ----------------------------------------------------------------------------------------------------
 *
 * 2. Why Zstd+GZIP?
 *
 * This was the best in terms of lossless algorithms and had a great tradeoff between effiency,
 * and fast compression + decompression times with MINIMAL dependencies.
 *
 * We are using a modified ZStandard algorithm taken from Facebook's ZSTD repository.
 *
 * The ZSTD compression is a C source file called into the dispatcher which is externed into this
 * dispatcher. It is then bundled into a `hls_data.tar.gz` (check macros.hpp)
 *
 * Also considering the fact that .ts are binary (octet-stream) data and .m3u8 is plain-text so
 * compression algorithm like ZSTD is perfect for this.
 *
 * Each transport stream on average had their content size reduced by ~30%
 * Each playlist file on average had their content size reduced by ~70+%
 *
 * This gives pretty good results overall, as the overall file size between simple compression using
 * TAR or GZIP, etc. do not achieve ZSTD+GZIP compression.
 *
 * Possible future for compression algorithms:
 *
 * -> AAC
 * -> Opus
 * -> Zstd + tar compression (most likely approach)
 *
 * AAC / OPUS or any other encoding will require additional dependencies in the server.
 *
 * ----------------------------------------------------------------------------------------------------
 *
 * 3. Why Boost C++?
 *
 * Boost provies ASIO -> Async I/O networking operations that allows for efficient handling of
 * multiple operations with minimal overhead that is quite scalable.
 *
 * Another great feature that Boost has is the OpenSSL support that manages SSL and HTTP
 * requests, all neatly wrapped back into Boost.Asio
 *
 * It is far more logical than pointless going with a scratch implementation or using low-level
 * networking headers and threads for a asynchronous operational server.
 *
 * ----------------------------------------------------------------------------------------------------
 *
 */

namespace beast = boost::beast;
namespace http  = beast::http;
namespace net   = boost::asio;
namespace ssl   = boost::asio::ssl;
namespace fs    = std::filesystem;
using tcp       = net::ip::tcp;

class Dispatcher
{
public:
  Dispatcher(std::string server, std::string port, std::string directory, std::string playlist_name)
      : ssl_ctx_(ssl::context::sslv23), resolver_(context_), stream_(context_, ssl_ctx_),
        server_(std::move(server)), port_(std::move(port)), directory_(std::move(directory)),
        playlist_name_(std::move(playlist_name))
  {
    if (!fs::exists(directory_))
    {
      LOG_ERROR << "[Dispatcher] Directory does not exist: " << directory_;
      throw std::runtime_error("Directory does not exist: " + directory_);
    }

    ssl_ctx_.set_default_verify_paths();
    stream_.set_verify_mode(boost::asio::ssl::verify_none); // [TODO]: Improve SSL verification
  }

  auto process_and_upload() -> bool
  {
    std::string master_playlist_path = fs::path(directory_) / playlist_name_;

    if (!verify_master_playlist(master_playlist_path))
    {
      LOG_ERROR << "[Dispatcher] Master playlist verification failed.";
      return false;
    }

    if (!verify_references())
    {
      LOG_ERROR << "[Dispatcher] Reference playlists or transport streams are invalid.";
      return false;
    }

#ifdef DEBUG_RUN
    print_hierarchy();
#endif

    std::string archive_path = fs::path(directory_) / macros::DISPATCH_ARCHIVE_NAME;
    if (!compress_files(archive_path))
    {
      LOG_ERROR << "[Dispatcher] Compression failed.";
      return false;
    }

    return upload_to_server(archive_path);
  }

private:
  net::io_context                context_;
  ssl::context                   ssl_ctx_;
  tcp::resolver                  resolver_;
  beast::ssl_stream<tcp::socket> stream_;
  std::string                    server_, port_, directory_, playlist_name_;

  std::unordered_map<std::string, std::vector<std::string>> reference_playlists_;
  std::vector<std::string>                                  transport_streams_;
  std::string                                               master_playlist_content_;

  auto verify_master_playlist(const std::string& path) -> bool
  {
    std::ifstream file(path);
    if (!file.is_open())
    {
      LOG_ERROR << "[Dispatcher] Failed to open master playlist: " << path;
      return false;
    }

    LOG_INFO << "[Dispatcher] Found master playlist: " << path;

    std::string line;
    bool        has_stream_inf = false;
    while (std::getline(file, line))
    {
      if (line.find(macros::PLAYLIST_VARIANT_TAG) != std::string::npos)
      {
        has_stream_inf = true;
        if (!std::getline(file, line) || line.empty() ||
            line.find(macros::PLAYLIST_EXT) == std::string::npos)
        {
          LOG_ERROR << "[Dispatcher] Invalid reference playlist in master.";
          return false;
        }
        std::string playlist_path           = fs::path(directory_) / line;
        reference_playlists_[playlist_path] = {}; // Store referenced playlists
        LOG_INFO << "[Dispatcher] Found reference playlist: " << playlist_path;
      }
    }

    if (!has_stream_inf)
    {
      LOG_WARNING << "[Dispatcher] No valid streams found in master playlist.";
      return false;
    }

    master_playlist_content_ = std::string(std::istreambuf_iterator<char>(file), {});
    LOG_INFO << "[Dispatcher] Master playlist verified successfully.";
    return true;
  }

  auto verify_references() -> bool
  {
    std::vector<std::string> mp4_segments_;
    std::vector<std::string> transport_streams_;

    for (auto& [playlist_path, segments] : reference_playlists_)
    {
      std::ifstream file(playlist_path);
      if (!file.is_open())
      {
        LOG_ERROR << "[Dispatcher] Missing referenced playlist: " << playlist_path;
        return false;
      }

      std::string line;
      enum class PlaylistFormat
      {
        UNKNOWN,
        TRANSPORT_STREAM,
        FMP4
      };
      PlaylistFormat playlist_format = PlaylistFormat::UNKNOWN;

      while (std::getline(file, line))
      {
        std::string segment_path = fs::path(directory_) / line;

        if (line.find(macros::TRANSPORT_STREAM_EXT) != std::string::npos)
        {
          if (playlist_format == PlaylistFormat::FMP4)
          {
            LOG_ERROR << "[Dispatcher] Inconsistent playlist format in: " << playlist_path
                      << " (Cannot mix .ts and .m4s segments)";
            return false;
          }
          playlist_format = PlaylistFormat::TRANSPORT_STREAM;

          std::ifstream ts_file(segment_path, std::ios::binary);
          if (!ts_file.is_open())
          {
            LOG_ERROR << "[Dispatcher] Failed to open transport stream: " << segment_path;
            return false;
          }

          char sync_byte;
          ts_file.read(&sync_byte, 1);
          if (sync_byte != TRANSPORT_STREAM_START_BYTE)
          {
            LOG_ERROR << "[Dispatcher] Invalid transport stream: " << segment_path
                      << " (Missing 0x47 sync byte)";
            return false;
          }

          segments.push_back(segment_path);
          transport_streams_.push_back(segment_path);
          LOG_INFO << "[Dispatcher] Found valid transport stream: " << segment_path;
        }
        else if (line.find(macros::M4S_FILE_EXT) != std::string::npos)
        {
          if (playlist_format == PlaylistFormat::TRANSPORT_STREAM)
          {
            LOG_ERROR << "[Dispatcher] Inconsistent playlist format in: " << playlist_path
                      << " (Cannot mix .ts and .m4s segments)";
            return false;
          }
          playlist_format = PlaylistFormat::FMP4;

          std::ifstream m4s_file(segment_path, std::ios::binary);
          if (!m4s_file.is_open())
          {
            LOG_ERROR << "[Dispatcher] Failed to open .m4s file: " << segment_path;
            return false;
          }

          if (!validate_m4s(segment_path))
          {
            LOG_WARNING << "[Dispatcher] M4S segment check failed: " << segment_path;
          }

          mp4_segments_.push_back(segment_path);
          LOG_INFO << "[Dispatcher] Found valid .m4s segment: " << segment_path;
        }
      }
    }

    // Ensure all verified segments exist in the filesystem
    for (const auto& ts : transport_streams_)
    {
      if (!fs::exists(ts))
      {
        LOG_ERROR << "[Dispatcher] Missing transport stream: " << ts;
        return false;
      }
    }

    for (const auto& m4s : mp4_segments_)
    {
      if (!fs::exists(m4s))
      {
        LOG_ERROR << "[Dispatcher] Missing .m4s segment: " << m4s;
        return false;
      }
    }

    LOG_INFO
      << "[Dispatcher] All referenced playlists and their respective segment types verified.";
    return true;
  }

  auto validate_m4s(const std::string& m4s_path) -> bool
  {
    std::ifstream file(m4s_path, std::ios::binary);
    if (!file.is_open())
    {
      LOG_ERROR << "[Dispatcher] Failed to open .m4s file: " << m4s_path;
      return false;
    }

    char header[8] = {0};
    file.read(header, 8); // Read the first 8 bytes

    if (std::string(header, 4) != "ftyp" && std::string(header, 4) != "moof")
    {
      LOG_ERROR << "[Dispatcher] Invalid .m4s file: " << m4s_path << " (Missing ftyp or moof box)";
      return false;
    }

    LOG_INFO << "[Dispatcher] Valid .m4s file: " << m4s_path;
    return true;
  }

  auto compress_files(const std::string& archive_path) -> bool
  {
    /* ZSTD_compressFilesInDirectory is a C source function (FFI) */
    if (!ZSTD_compressFilesInDirectory(
          fs::path(directory_).c_str(),
          macros::to_string(macros::DISPATCH_ARCHIVE_REL_PATH).c_str()))
    {
      LOG_ERROR << "[Dispatcher] Something went wrong with Zstd compression.";
      return false;
    }

    struct archive* archive = archive_write_new();
    archive_write_add_filter_gzip(archive);
    archive_write_set_format_pax_restricted(archive);

    if (archive_write_open_filename(archive, archive_path.c_str()) != ARCHIVE_OK)
    {
      LOG_ERROR << "[Dispatcher] Failed to create archive: " << archive_path;
      return false;
    }

    auto add_file_to_archive = [&](const std::string& file_path) -> bool
    {
      std::ifstream file(file_path, std::ios::binary);
      if (!file)
      {
        LOG_ERROR << "[Dispatcher] Failed to open file: " << file_path;
        return false;
      }

      std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
      struct archive_entry* entry = archive_entry_new();
      archive_entry_set_pathname(entry, fs::path(file_path).filename().string().c_str());
      archive_entry_set_size(entry, content.size());
      archive_entry_set_filetype(entry, AE_IFREG);
      archive_entry_set_perm(entry, 0644);
      archive_write_header(archive, entry);
      archive_write_data(archive, content.data(), content.size());
      archive_entry_free(entry);
      return true;
    };

    for (const auto& entry : fs::directory_iterator(fs::path(macros::DISPATCH_ARCHIVE_REL_PATH)))
    {
      if (entry.is_regular_file())
      {
        if (!add_file_to_archive(entry.path().string()))
        {
          LOG_ERROR << "[Dispatcher] Failed to add file: " << entry.path();
          archive_write_close(archive);
          archive_write_free(archive);
          return false;
        }
      }
    }

    // Close the archive
    if (archive_write_close(archive) != ARCHIVE_OK)
    {
      LOG_ERROR << "[Dispatcher] Failed to close archive properly";
      archive_write_free(archive);
      return false;
    }

    archive_write_free(archive);
    LOG_INFO << "[Dispatcher] ZSTD compression of " << directory_ << " to " << archive_path
             << " with final GNU tar job done.";
    return true;
  }

  auto upload_to_server(const std::string& archive_path) -> bool
  {
    try
    {
      auto const results = resolver_.resolve(server_, port_);
      net::connect(stream_.next_layer(), results.begin(), results.end());
      stream_.handshake(ssl::stream_base::client);

      send_http_request("POST", archive_path);

      LOG_INFO << "[Dispatcher] Upload process completed successfully.";
      return true;
    }
    catch (const std::exception& e)
    {
      LOG_ERROR << "[Dispatcher] Upload failed: " << e.what();
      return false;
    }
  }

  void send_http_request(const std::string& method, const std::string& archive_path)
  {
    std::ifstream file(archive_path, std::ios::binary);
    std::string   content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    http::request<http::string_body> req{http::verb::post, "/", 11};
    req.set(http::field::host, server_);
    req.set(http::field::content_type, macros::CONTENT_TYPE_COMPRESSION);
    req.set(http::field::content_disposition,
            "attachment; filename=\"" + fs::path(archive_path).filename().string() + "\"");
    req.body() = content;
    req.prepare_payload();

    try
    {
      http::write(stream_, req);
      beast::flat_buffer                buffer;
      http::response<http::string_body> res;
      http::read(stream_, buffer, res);

      if (res.result() != http::status::ok)
      {
        LOG_ERROR << "[Dispatcher] Server error: " << res.result_int();
      }
      else
      {
        LOG_INFO << "[Dispatcher] Successfully uploaded archive.";
      }
    }
    catch (const std::exception& e)
    {
      LOG_ERROR << "[Dispatcher] HTTP request failed: " << e.what();
    }
  }

  void print_hierarchy()
  {
    LOG_INFO << "\n HLS Playlist Hierarchy:\n";
    std::cout << ">> " << playlist_name_ << "\n";

    for (const auto& [playlist, segments] : reference_playlists_)
    {
      std::cout << "   ├── > " << fs::path(playlist).filename().string() << "\n";
      for (const auto& ts : segments)
      {
        std::cout << "   │   ├── @ " << fs::path(ts).filename().string() << "\n";
      }
    }
  }
};

auto main(int argc, char* argv[]) -> int
{
  logger::init_logging();

  if (argc < 5)
  {
    LOG_ERROR << "Usage: " << argv[0] << " <server> <port> <directory> <master_playlist>";
    return 1;
  }

  std::string server          = argv[1];
  std::string port            = argv[2];
  std::string dir             = argv[3];
  std::string master_playlist = argv[4];

  try
  {
    Dispatcher dispatcher(server, port, dir, master_playlist);
    if (!dispatcher.process_and_upload())
    {
      LOG_ERROR << "[Main] Upload process failed.";
      return 1;
    }

    LOG_INFO << "[Main] Upload successful.";
    return 0;
  }
  catch (const std::exception& e)
  {
    LOG_ERROR << "[Main] Error: " << e.what();
    return 1;
  }
}
