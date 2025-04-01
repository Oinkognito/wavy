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

#include <libwavy/common/macros.hpp>
#include <libwavy/logger.hpp>
#include <libwavy/zstd/compression.h>

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

namespace libwavy::dispatch
{

enum class PlaylistFormat
{
  UNKNOWN,
  TRANSPORT_STREAM,
  FMP4
};

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
      LOG_ERROR << DISPATCH_LOG << "Directory does not exist: " << directory_;
      throw std::runtime_error("Directory does not exist: " + directory_);
    }

    ssl_ctx_.set_default_verify_paths();
    stream_.set_verify_mode(boost::asio::ssl::verify_none); // [TODO]: Improve SSL verification
  }

  auto process_and_upload() -> bool
  {
    if (fs::exists(macros::DISPATCH_ARCHIVE_REL_PATH))
    {
      LOG_DEBUG << DISPATCH_LOG << "Payload already exists, checking for "
                << macros::DISPATCH_ARCHIVE_NAME << "...";
      std::string archive_path = fs::path(directory_) / macros::DISPATCH_ARCHIVE_NAME;
      if (fs::exists(archive_path))
        return upload_to_server(archive_path);
    }

    std::string master_playlist_path = fs::path(directory_) / playlist_name_;

    if (!verify_master_playlist(master_playlist_path))
    {
      LOG_ERROR << DISPATCH_LOG << "Master playlist verification failed.";
      return false;
    }

    if (!verify_references())
    {
      LOG_ERROR << DISPATCH_LOG << "Reference playlists or transport streams are invalid.";
      return false;
    }

    std::string metadata_path = fs::path(directory_) / macros::to_string(macros::METADATA_FILE);
    if (!fs::exists(metadata_path))
    {
      LOG_ERROR << DISPATCH_LOG << "Missing metadata.toml in: " << directory_;
      return false;
    }
    LOG_INFO << DISPATCH_LOG << "Found metadata.toml: " << metadata_path;

#ifdef DEBUG_RUN
    print_hierarchy();
#endif

    std::string archive_path  = fs::path(directory_) / macros::DISPATCH_ARCHIVE_NAME;
    bool        applyZSTDComp = true;
    if (playlist_format == PlaylistFormat::FMP4)
    {
      LOG_DEBUG << DISPATCH_LOG
                << "Found FMP4 files, no point in compressing them. Skipping ZSTD "
                   "compression job";
      applyZSTDComp = false;
    }

    if (!compress_files(archive_path, applyZSTDComp))
    {
      LOG_ERROR << DISPATCH_LOG << "Compression failed.";
      return false;
    }

    return upload_to_server(archive_path);
  }

private:
  net::io_context                context_;
  PlaylistFormat                 playlist_format = PlaylistFormat::UNKNOWN;
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
      LOG_ERROR << DISPATCH_LOG << "Failed to open master playlist: " << path;
      return false;
    }

    LOG_INFO << DISPATCH_LOG << "Found master playlist: " << path;

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
          LOG_ERROR << DISPATCH_LOG << "Invalid reference playlist in master.";
          return false;
        }
        std::string playlist_path           = fs::path(directory_) / line;
        reference_playlists_[playlist_path] = {}; // Store referenced playlists
        LOG_INFO << DISPATCH_LOG << "Found reference playlist: " << playlist_path;
      }
    }

    if (!has_stream_inf)
    {
      LOG_WARNING << DISPATCH_LOG << "No valid streams found in master playlist.";
      return false;
    }

    master_playlist_content_ = std::string(std::istreambuf_iterator<char>(file), {});
    LOG_INFO << DISPATCH_LOG << "Master playlist verified successfully.";
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
        LOG_ERROR << DISPATCH_LOG << "Missing referenced playlist: " << playlist_path;
        return false;
      }

      std::string line;

      while (std::getline(file, line))
      {
        std::string segment_path = fs::path(directory_) / line;

        if (line.find(macros::TRANSPORT_STREAM_EXT) != std::string::npos)
        {
          if (playlist_format == PlaylistFormat::FMP4)
          {
            LOG_ERROR << DISPATCH_LOG << "Inconsistent playlist format in: " << playlist_path
                      << " (Cannot mix .ts and .m4s segments)";
            return false;
          }
          playlist_format = PlaylistFormat::TRANSPORT_STREAM;

          std::ifstream ts_file(segment_path, std::ios::binary);
          if (!ts_file.is_open())
          {
            LOG_ERROR << DISPATCH_LOG << "Failed to open transport stream: " << segment_path;
            return false;
          }

          char sync_byte;
          ts_file.read(&sync_byte, 1);
          if (sync_byte != TRANSPORT_STREAM_START_BYTE)
          {
            LOG_ERROR << DISPATCH_LOG << "Invalid transport stream: " << segment_path
                      << " (Missing 0x47 sync byte)";
            return false;
          }

          segments.push_back(segment_path);
          transport_streams_.push_back(segment_path);
          LOG_INFO << DISPATCH_LOG << "Found valid transport stream: " << segment_path;
        }
        else if (line.find(macros::M4S_FILE_EXT) != std::string::npos)
        {
          if (playlist_format == PlaylistFormat::TRANSPORT_STREAM)
          {
            LOG_ERROR << DISPATCH_LOG << "Inconsistent playlist format in: " << playlist_path
                      << " (Cannot mix .ts and .m4s segments)";
            return false;
          }
          playlist_format = PlaylistFormat::FMP4;

          std::ifstream m4s_file(segment_path, std::ios::binary);
          if (!m4s_file.is_open())
          {
            LOG_ERROR << DISPATCH_LOG << "Failed to open .m4s file: " << segment_path;
            return false;
          }

          if (!validate_m4s(segment_path))
          {
            LOG_WARNING << DISPATCH_LOG << "M4S segment check failed: " << segment_path;
          }

          mp4_segments_.push_back(segment_path);
          LOG_INFO << DISPATCH_LOG << "Found valid .m4s segment: " << segment_path;
        }
      }
    }

    // Ensure all verified segments exist in the filesystem
    for (const auto& ts : transport_streams_)
    {
      if (!fs::exists(ts))
      {
        LOG_ERROR << DISPATCH_LOG << "Missing transport stream: " << ts;
        return false;
      }
    }

    for (const auto& m4s : mp4_segments_)
    {
      if (!fs::exists(m4s))
      {
        LOG_ERROR << DISPATCH_LOG << "Missing .m4s segment: " << m4s;
        return false;
      }
    }

    LOG_INFO << DISPATCH_LOG
             << "All referenced playlists and their respective segment types verified.";
    return true;
  }

  auto validate_m4s(const std::string& m4s_path) -> bool
  {
    std::ifstream file(m4s_path, std::ios::binary);
    if (!file.is_open())
    {
      LOG_ERROR << DISPATCH_LOG << "Failed to open .m4s file: " << m4s_path;
      return false;
    }

    char header[8] = {0};
    file.read(header, 8); // Read the first 8 bytes

    if (std::string(header, 4) != "ftyp" && std::string(header, 4) != "moof")
    {
      return false;
    }

    LOG_INFO << DISPATCH_LOG << "Valid .m4s file: " << m4s_path;
    return true;
  }

  auto compress_files(const std::string& output_archive_path, const bool applyZSTDComp) -> bool
  {
    LOG_DEBUG << DISPATCH_LOG << "Beginning Compression Job in: " << output_archive_path << " from "
              << fs::absolute(directory_);
    /* ZSTD_compressFilesInDirectory is a C source function (FFI) */
    if (applyZSTDComp)
    {
      if (!ZSTD_compressFilesInDirectory(
            fs::path(directory_).c_str(),
            macros::to_string(macros::DISPATCH_ARCHIVE_REL_PATH).c_str()))
      {
        LOG_ERROR << DISPATCH_LOG << "Something went wrong with Zstd compression.";
        return false;
      }
    }

    struct archive* archive = archive_write_new();
    archive_write_add_filter_gzip(archive);
    archive_write_set_format_pax_restricted(archive);

    if (archive_write_open_filename(archive, output_archive_path.c_str()) != ARCHIVE_OK)
    {
      LOG_ERROR << DISPATCH_LOG << "Failed to create archive: " << output_archive_path;
      return false;
    }

    auto add_file_to_archive = [&](const std::string& file_path) -> bool
    {
      std::ifstream file(file_path, std::ios::binary);
      if (!file)
      {
        LOG_ERROR << DISPATCH_LOG << "Failed to open file: " << file_path;
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

    /* Payload directory is only created if we compress each stream segment into
     * ZSTD files.
     *
     * Since MP4 and M4S files have minimal viability for compression, we just ignore
     * that operation, meaning the payload target is now just directory_ variable.
     */
    fs::path payloadTarget =
      applyZSTDComp ? fs::path(macros::DISPATCH_ARCHIVE_REL_PATH) : fs::path(directory_);

    LOG_DEBUG << DISPATCH_LOG << "Making payload target: " << payloadTarget;

    for (const auto& entry : fs::directory_iterator(payloadTarget))
    {
      if (entry.is_regular_file())
      {
        if (!add_file_to_archive(entry.path().string()))
        {
          LOG_ERROR << DISPATCH_LOG << "Failed to add file: " << entry.path();
          archive_write_close(archive);
          archive_write_free(archive);
          return false;
        }
      }
    }

    // Close the archive
    if (archive_write_close(archive) != ARCHIVE_OK)
    {
      LOG_ERROR << DISPATCH_LOG << "Failed to close archive properly";
      archive_write_free(archive);
      return false;
    }

    archive_write_free(archive);
    LOG_INFO << DISPATCH_LOG << "ZSTD compression of " << directory_ << " to "
             << output_archive_path << " with final GNU tar job done.";
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

      // **Proper SSL stream shutdown**
      boost::system::error_code ec;
      stream_.shutdown(ec);
      if (ec == boost::asio::error::eof)
      {
        // Expected, means server closed the connection cleanly
        ec.clear();
      }
      else if (ec)
      {
        LOG_ERROR << DISPATCH_LOG << "SSL shutdown failed: " << ec.message();
      }

      LOG_INFO << DISPATCH_LOG << "Upload process completed successfully.";
      return true;
    }
    catch (const std::exception& e)
    {
      LOG_ERROR << DISPATCH_LOG << "Upload failed: " << e.what();
      return false;
    }
  }

  void send_http_request(const std::string& method, const std::string& archive_path)
  {
    beast::error_code                         ec;
    boost::beast::http::file_body::value_type body;
    body.open(archive_path.c_str(), beast::file_mode::scan, ec);
    if (ec)
    {
      LOG_ERROR << DISPATCH_LOG << "Failed to open archive file: " << archive_path;
      return;
    }

    http::request<http::file_body> req{http::string_to_verb(method), "/", 11};
    req.set(http::field::host, server_);
    req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
    req.set(http::field::content_type, "application/gzip");
    req.body() = std::move(body);
    req.prepare_payload();

    http::write(stream_, req, ec);
    if (ec)
    {
      LOG_ERROR << DISPATCH_LOG << "Failed to send request: " << ec.message();
      return;
    }

    // Read response from server
    beast::flat_buffer                buffer;
    http::response<http::string_body> res;
    http::read(stream_, buffer, res, ec);

    if (ec)
    {
      LOG_ERROR << DISPATCH_LOG << "Failed to read response: " << ec.message();
      return;
    }

    // Extract and log Client-ID separately
    auto client_id_it = res.find("Client-ID");
    if (client_id_it != res.end())
    {
      LOG_INFO << "Parsed Client-ID: " << client_id_it->value();
    }
    else
    {
      LOG_WARNING << "Client-ID not found in response headers.";
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

} // namespace libwavy::dispatch
