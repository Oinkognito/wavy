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
 * ... [Documentation remains the same] ...
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
private:
  // Track FLAC status per playlist
  std::unordered_map<std::string, bool> playlist_is_flac_;
  // Store compression mode: "lossy" or "lossless"
  std::string compression_mode_;
  
public:
  // Updated constructor to take compression mode as an extra argument.
  Dispatcher(std::string server, std::string port, std::string directory, 
             std::string playlist_name, std::string compression_mode)
      : ssl_ctx_(ssl::context::sslv23), resolver_(context_), stream_(context_, ssl_ctx_),
        server_(std::move(server)), port_(std::move(port)), directory_(std::move(directory)),
        playlist_name_(std::move(playlist_name)), compression_mode_(std::move(compression_mode))
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
    bool has_stream_inf = false;
    while (std::getline(file, line))
    {
      if (line.find("#EXT-X-STREAM-INF:") != std::string::npos)
      {
        has_stream_inf = true;
        bool is_flac = (line.find("flac") != std::string::npos);

        if (!std::getline(file, line) || line.empty())
        {
          LOG_ERROR << "[Dispatcher] Invalid reference playlist in master.";
          return false;
        }

        std::string playlist_path = fs::path(directory_) / line;
        playlist_is_flac_[playlist_path] = is_flac; // Store FLAC status
        reference_playlists_[playlist_path] = {};    // Store referenced playlists

        LOG_INFO << "[Dispatcher] Found " << (is_flac ? "FLAC" : "MP3")
                 << " reference playlist: " << playlist_path;
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
    for (auto& [playlist_path, segments] : reference_playlists_)
    {
      bool is_flac = playlist_is_flac_[playlist_path];

      std::ifstream file(playlist_path);
      if (!file.is_open())
      {
        LOG_ERROR << "[Dispatcher] Missing referenced playlist: " << playlist_path;
        return false;
      }

      std::string line;
      while (std::getline(file, line))
      {
        // Handle both TS and fMP4 segments
        if (line.find(".ts") != std::string::npos || line.find(".m4s") != std::string::npos)
        {
          std::string segment_path = fs::path(directory_) / line;

          if (!is_flac)
          {
            // Verify TS sync byte only for non-FLAC streams
            std::ifstream ts_file(segment_path, std::ios::binary);
            char sync_byte;
            ts_file.read(&sync_byte, 1);
            if (sync_byte != TRANSPORT_STREAM_START_BYTE)
            {
              LOG_ERROR << "[Dispatcher] Invalid transport stream: " << segment_path
                        << " (Missing 0x47 sync byte)";
              return false;
            }
          }

          segments.push_back(segment_path);
          transport_streams_.push_back(segment_path);
          LOG_INFO << "[Dispatcher] Found valid " 
                   << (is_flac ? "fMP4" : "TS") << " segment: " << segment_path;
        }
      }
    }

    for (const auto& ts : transport_streams_)
    {
      if (!fs::exists(ts))
      {
        LOG_ERROR << "[Dispatcher] Missing transport stream: " << ts;
        return false;
      }
    }

    LOG_INFO << "[Dispatcher] All referenced playlists and transport streams verified.";
    return true;
  }

  // Updated compression: if compression_mode_ is "lossy", run ZSTD compression; if "lossless", skip it.
  auto compress_files(const std::string& archive_path) -> bool
  {
    // Determine which directory to archive.
    std::string source_to_archive = directory_;
    
    if (compression_mode_ == "lossy")
    {
      // Run ZSTD compression into a temporary directory defined by DISPATCH_ARCHIVE_REL_PATH.
      if (!ZSTD_compressFilesInDirectory(
            fs::path(directory_).c_str(),
            macros::to_string(macros::DISPATCH_ARCHIVE_REL_PATH).c_str()))
      {
        LOG_ERROR << "[Dispatcher] Something went wrong with Zstd compression.";
        return false;
      }
      // Use the output of the ZSTD compression as the source for archiving.
      source_to_archive = macros::to_string(macros::DISPATCH_ARCHIVE_REL_PATH);
    }
    else if (compression_mode_ == "lossless")
    {
      LOG_INFO << "[Dispatcher] Lossless mode selected: skipping ZSTD compression.";
    }
    else
    {
      LOG_ERROR << "[Dispatcher] Unknown compression mode: " << compression_mode_;
      return false;
    }

    // Now create the archive (using gzip) from source_to_archive.
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

    // Archive only files with .ts, .m4s, or .m3u8 from the chosen source directory.
    for (const auto& entry : fs::directory_iterator(fs::path(source_to_archive)))
    {
      if (entry.is_regular_file())
      {
        const std::string ext = entry.path().extension().string();
        if (ext == ".ts" || ext == ".m4s" || ext == ".m3u8")
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
    }

    // Close the archive
    if (archive_write_close(archive) != ARCHIVE_OK)
    {
      LOG_ERROR << "[Dispatcher] Failed to close archive properly";
      archive_write_free(archive);
      return false;
    }

    archive_write_free(archive);
    LOG_INFO << "[Dispatcher] Compression of " << source_to_archive << " to " 
             << archive_path << " completed.";
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

  // Now require 5 arguments: server, port, directory, master_playlist, and compression_mode.
  if (argc < 6)
  {
    LOG_ERROR << "Usage: " << argv[0] << " <server> <port> <directory> <master_playlist> <compression_mode (lossy|lossless)>";
    return 1;
  }

  std::string server          = argv[1];
  std::string port            = argv[2];
  std::string dir             = argv[3];
  std::string master_playlist = argv[4];
  std::string comp_mode       = argv[5];

  try
  {
    Dispatcher dispatcher(server, port, dir, master_playlist, comp_mode);
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
