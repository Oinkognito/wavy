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
#include <libwavy/common/api/entry.hpp>
#include <unordered_map>

#include <libwavy/common/macros.hpp>
#include <libwavy/common/network/routes.h>
#include <libwavy/common/state.hpp>
#include <libwavy/common/types.hpp>
#include <libwavy/log-macros.hpp>
#include <libwavy/utils/math/entry.hpp>
#include <libwavy/zstd/compression.h>

#include <indicators/cursor_control.hpp>
#include <indicators/progress_bar.hpp>
#include <indicators/setting.hpp>

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
namespace asio  = boost::asio;
namespace ssl   = boost::asio::ssl;
namespace fs    = std::filesystem;
using tcp       = asio::ip::tcp;

using Dispatch = libwavy::log::DISPATCH;
using Socket   = beast::ssl_stream<tcp::socket>;

namespace libwavy::dispatch
{

enum class PlaylistFormat
{
  UNKNOWN,
  TRANSPORT_STREAM,
  FMP4
};

class WAVY_API Dispatcher
{
public:
  Dispatcher(IPAddr server, StorageOwnerID nickname, Directory directory, FileName playlist_name)
      : m_sslCtx(ssl::context::sslv23), m_resolver(m_ioCtx), m_socket(m_ioCtx, m_sslCtx),
        m_server(std::move(server)), m_nickname(std::move(nickname)),
        m_directory(std::move(directory)), m_playlistName(std::move(playlist_name))
  {
    if (!fs::exists(m_directory))
    {
      log::ERROR<Dispatch>("Directory does not exist: {}", m_directory);
      throw std::runtime_error("Directory does not exist: " + m_directory);
    }

    const fs::path m_nicknamefile_path =
      fs::path(m_directory) / (m_nickname + macros::to_string(macros::OWNER_FILE_EXT));
    std::ofstream file(m_nicknamefile_path);
    if (!file)
    {
      log::ERROR<Dispatch>("Failed to create file: '{}'", m_nicknamefile_path.string());
      throw std::runtime_error("Failed to create file: " + m_nicknamefile_path.string());
    }
    file << "Created for user: " << m_nickname << "\n";
    file.close();

    m_sslCtx.set_default_verify_paths();
    m_socket.set_verify_mode(ssl::verify_none); // [TODO]: Improve SSL verification
  }

  auto process_and_upload() -> bool
  {
    if (fs::exists(macros::DISPATCH_ARCHIVE_REL_PATH))
    {
      log::DBG<Dispatch>("Payload already exists, checking for {}...",
                         macros::DISPATCH_ARCHIVE_NAME);
      AbsPath archive_path = fs::path(m_directory) / macros::DISPATCH_ARCHIVE_NAME;
      if (fs::exists(archive_path))
        return upload_to_server(archive_path);
    }

    const AbsPath master_playlist_path = fs::path(m_directory) / m_playlistName;

    if (!verify_master_playlist(master_playlist_path))
    {
      log::ERROR<Dispatch>("Master playlist verification failed!");
      return false;
    }

    if (!verify_references())
    {
      log::ERROR<Dispatch>("Reference playlists or transport streams are invalid.");
      return false;
    }

    const AbsPath metadata_path = fs::path(m_directory) / macros::to_string(macros::METADATA_FILE);
    if (!fs::exists(metadata_path))
    {
      log::ERROR<Dispatch>("Missing metadata.toml in: {}", m_directory);
      return false;
    }
    log::INFO<Dispatch>("Found metadata.toml: {}", metadata_path);

#ifdef DEBUG_RUN
    print_hierarchy();
#endif

    const AbsPath archive_path  = fs::path(m_directory) / macros::DISPATCH_ARCHIVE_NAME;
    bool          applyZSTDComp = true;
    if (m_playlistFmt == PlaylistFormat::FMP4)
    {
      log::DBG<Dispatch>(
        "Found FMP4 files, no point in compressing them. Skipping ZSTD compression job..");
      applyZSTDComp = false;
    }

    if (!compress_files(archive_path, applyZSTDComp))
    {
      log::ERROR<Dispatch>("Compression failed!!");
      return false;
    }

    return upload_to_server(archive_path);
  }

private:
  asio::io_context m_ioCtx;
  PlaylistFormat   m_playlistFmt = PlaylistFormat::UNKNOWN;
  ssl::context     m_sslCtx;
  tcp::resolver    m_resolver;
  Socket           m_socket;
  IPAddr           m_server;
  DirPathHolder    m_directory, m_playlistName;
  StorageOwnerID   m_nickname;

  std::unordered_map<AbsPath, TotalAudioData> m_refPlaylists;
  TotalAudioData                              m_transportStreams;
  PlaylistData                                m_masterPlaylistContent;

  auto verify_master_playlist(const AbsPath& path) -> bool
  {
    std::ifstream file(path);
    if (!file.is_open())
    {
      log::ERROR<Dispatch>("Failed to open master playlist: '{}'", path);
      return false;
    }

    log::INFO<Dispatch>("Found master playlist: '{}'!", path);

    std::string line;
    bool        has_m_socketinf = false;
    while (std::getline(file, line))
    {
      if (line.find(macros::PLAYLIST_VARIANT_TAG) != std::string::npos)
      {
        has_m_socketinf = true;
        if (!std::getline(file, line) || line.empty() ||
            line.find(macros::PLAYLIST_EXT) == std::string::npos)
        {
          log::ERROR<Dispatch>("Invalid reference playlist in master!");
          return false;
        }
        const AbsPath playlist_path   = fs::path(m_directory) / line;
        m_refPlaylists[playlist_path] = {}; // Store referenced playlists
        log::INFO<Dispatch>("Found reference playlist: {}", playlist_path);
      }
    }

    if (!has_m_socketinf)
    {
      log::WARN<Dispatch>("No valid streams found in master playlist!");
      return false;
    }

    m_masterPlaylistContent = PlaylistData(std::istreambuf_iterator<char>(file), {});
    log::INFO<Dispatch>("Master playlist verified successfully.");
    return true;
  }

  auto verify_references() -> bool
  {
    TotalAudioData mp4_segments_, m_transportStreams;

    for (auto& [playlist_path, segments] : m_refPlaylists)
    {
      std::ifstream file(playlist_path);
      if (!file.is_open())
      {
        log::ERROR<Dispatch>("Missing referenced playlist: {}", playlist_path);
        return false;
      }

      std::string line;

      while (std::getline(file, line))
      {
        const AbsPath segment_path = fs::path(m_directory) / line;

        if (line.find(macros::TRANSPORT_STREAM_EXT) != std::string::npos)
        {
          if (m_playlistFmt == PlaylistFormat::FMP4)
          {
            log::ERROR<Dispatch>(
              "Inconsistent playlist format in: {} (Cannot mix .ts and .m4s segments!!)",
              playlist_path);
            return false;
          }
          m_playlistFmt = PlaylistFormat::TRANSPORT_STREAM;

          std::ifstream ts_file(segment_path, std::ios::binary);
          if (!ts_file.is_open())
          {
            log::ERROR<Dispatch>("Failed to open transport stream: {}", segment_path);
            return false;
          }

          char sync_byte;
          ts_file.read(&sync_byte, 1);
          if (sync_byte != TRANSPORT_STREAM_START_BYTE)
          {
            log::ERROR<Dispatch>("Invalid transport stream: {} (Missing 0x47 sync byte)!",
                                 segment_path);
            return false;
          }

          segments.push_back(segment_path);
          m_transportStreams.push_back(segment_path);
          log::TRACE<Dispatch>("Found valid transport stream: {}", segment_path);
        }
        else if (line.find(macros::M4S_FILE_EXT) != std::string::npos)
        {
          if (m_playlistFmt == PlaylistFormat::TRANSPORT_STREAM)
          {
            log::ERROR<Dispatch>(
              "Inconsistent playlist format in: {} (Cannot mix .ts and .m4s segments!!)",
              playlist_path);
            return false;
          }
          m_playlistFmt = PlaylistFormat::FMP4;

          std::ifstream m4s_file(segment_path, std::ios::binary);
          if (!m4s_file.is_open())
          {
            log::ERROR<Dispatch>("Failed to open .m4s file: {}", segment_path);
            return false;
          }

          /* ------ DEPRECATED API -------- */
          if (!validate_m4s(segment_path))
          {
            // [TODO] Add logs but in the future will have better solution for this.
          }

          mp4_segments_.push_back(segment_path);
          log::TRACE<Dispatch>("Found valid .m4s segment: {}", segment_path);
        }
      }
    }

    // Ensure all verified segments exist in the filesystem
    for (const auto& ts : m_transportStreams)
    {
      if (!fs::exists(ts))
      {
        log::ERROR<Dispatch>("Missing transport stream: {}", ts);
        return false;
      }
    }

    for (const auto& m4s : mp4_segments_)
    {
      if (!fs::exists(m4s))
      {
        log::ERROR<Dispatch>("Missing .m4s segment: {}", m4s);
        return false;
      }
    }

    log::INFO<Dispatch>("All referenced playlists and their respective segment types verified.");
    log::INFO<Dispatch>("Found {} verified transport streams.",
                        (m_playlistFmt == PlaylistFormat::TRANSPORT_STREAM
                           ? m_transportStreams.size()
                           : mp4_segments_.size()));
    return true;
  }

  WAVY_DEPRECATED("Validating an M4S file is coming soon!")
  auto validate_m4s(const AbsPath& m4s_path) -> bool
  {
    // std::ifstream file(m4s_path, std::ios::binary);
    // if (!file.is_open())
    // {
    //   LOG_ERROR << DISPATCH_LOG << "Failed to open .m4s file: " << m4s_path;
    //   return false;
    // }
    //
    // char header[8] = {0};
    // file.read(header, 8); // Read the first 8 bytes
    //
    // if (std::string(header, 4) != "ftyp" && std::string(header, 4) != "moof")
    // {
    //   return false;
    // }

    return true;
  }

  auto compress_files(const AbsPath& output_archive_path, const bool applyZSTDComp) -> bool
  {
    log::DBG<Dispatch>("Beginning Compression Job in: {} from {}", output_archive_path,
                       fs::absolute(m_directory).string());
    /* ZSTD_compressFilesInDirectory is a C source function (FFI) */
    if (applyZSTDComp)
    {
      if (!ZSTD_compressFilesInDirectory(
            fs::path(m_directory).c_str(),
            macros::to_string(macros::DISPATCH_ARCHIVE_REL_PATH).c_str()))
      {
        log::ERROR<Dispatch>("Something went wrong with ZSTD compression!");
        return false;
      }
    }

    struct archive* archive = archive_write_new();
    archive_write_add_filter_gzip(archive);
    archive_write_set_format_pax_restricted(archive);

    if (archive_write_open_filename(archive, output_archive_path.c_str()) != ARCHIVE_OK)
    {
      log::ERROR<Dispatch>("Failed to create archive: {}", output_archive_path);
      return false;
    }

    auto AddFileToArchive = [&](const RelPath& file_path) -> bool
    {
      std::ifstream file(file_path, std::ios::binary);
      if (!file)
      {
        log::ERROR<Dispatch>("Failed to open file: {}", file_path);
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
     * that operation, meaning the payload target is now just m_directory variable.
     */
    const fs::path payloadTarget =
      applyZSTDComp ? fs::path(macros::DISPATCH_ARCHIVE_REL_PATH) : fs::path(m_directory);

    log::DBG<Dispatch>("Making payload target: {}", payloadTarget.string());

    for (const auto& entry : fs::directory_iterator(payloadTarget))
    {
      if (entry.is_regular_file())
      {
        if (!AddFileToArchive(entry.path().string()))
        {
          log::ERROR<Dispatch>("Failed to add file: {}", entry.path().string());
          archive_write_close(archive);
          archive_write_free(archive);
          return false;
        }
      }
    }

    // Close the archive
    if (archive_write_close(archive) != ARCHIVE_OK)
    {
      log::ERROR<Dispatch>("Failed to close archive properly!!");
      archive_write_free(archive);
      return false;
    }

    archive_write_free(archive);
    log::INFO<Dispatch>("ZSTD compression of {} to {} with final GNU TAR job done.", m_directory,
                        output_archive_path);
    return true;
  }

  auto upload_to_server(const AbsPath& archive_path) -> bool
  {
    try
    {
      auto const results = m_resolver.resolve(m_server, WAVY_SERVER_PORT_NO_STR);
      asio::connect(m_socket.next_layer(), results.begin(), results.end());
      m_socket.handshake(ssl::stream_base::client);

      std::ifstream file(archive_path, std::ios::binary | std::ios::ate);
      if (!file.is_open())
      {
        log::ERROR<Dispatch>("Could not open file for upload: {}", archive_path);
        return false;
      }

      const auto file_size = file.tellg();
      file.seekg(0);

      log::INFO<Dispatch>("Dispatching to Wavy Server....");

      indicators::ProgressBar bar{
        indicators::option::BarWidth{50}, indicators::option::MaxProgress{100},
        indicators::option::ShowElapsedTime{true}, indicators::option::ShowRemainingTime{true},
        indicators::option::FontStyles{
          std::vector<indicators::FontStyle>{indicators::FontStyle::bold}}};

      beast::flat_buffer buffer;

      http::request<http::empty_body> req{http::verb::post, routes::SERVER_PATH_TOML_UPLOAD, 11};
      req.set(http::field::host, m_server);
      req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
      req.set(http::field::transfer_encoding, "chunked");
      req.set(http::field::content_type, macros::CONTENT_TYPE_GZIP);

      http::serializer<true, http::empty_body> sr{req};
      http::write_header(m_socket, sr);

      constexpr std::size_t CHUNK_SIZE = 64 * 1024; // 64 KB
      std::vector<char>     buffer_chunk(CHUNK_SIZE);
      std::size_t           total_sent = 0;

      while (file)
      {
        file.read(buffer_chunk.data(), CHUNK_SIZE);
        std::streamsize bytes_read = file.gcount();

        if (bytes_read <= 0)
          break;

        std::ostringstream chunk_stream;
        chunk_stream << std::hex << bytes_read << "\r\n";
        chunk_stream.write(buffer_chunk.data(), bytes_read);
        chunk_stream << "\r\n";

        std::string chunk_data = chunk_stream.str();
        asio::write(m_socket, asio::buffer(chunk_data));

        total_sent += bytes_read;
        bar.set_progress(static_cast<ui32>((total_sent * 100) / file_size));
      }

      // Final zero-length chunk to indicate end
      asio::write(m_socket, asio::buffer("0\r\n\r\n"));

      http::response<http::string_body> res;
      http::read(m_socket, buffer, res);

      if (res.result() != http::status::ok)
      {
        log::ERROR<Dispatch>("Upload failed: {}", res.result_int());
        return false;
      }

      if (auto audio_id_it = res.find("Audio-ID"); audio_id_it != res.end())
      {
        log::INFO<Dispatch>("Parsed Audio-ID: {}", std::string(audio_id_it->value()));
      }

      log::INFO<Dispatch>("Upload completed successfully ({} sent)",
                          utils::math::bytesFormat(total_sent));
      m_socket.shutdown();

      return true;
    }
    catch (const std::exception& e)
    {
      log::ERROR<Dispatch>("Upload exception: {}", e.what());
      return false;
    }
  }

  void send_http_request(NetMethods& method, const AbsPath& archive_path)
  {
    beast::error_code                  ec;
    beast::http::file_body::value_type body;
    body.open(archive_path.c_str(), beast::file_mode::scan, ec);
    if (ec)
    {
      log::ERROR<Dispatch>("Failed to open archive file: {}", archive_path);
      return;
    }

    http::request<http::file_body> req{http::string_to_verb(method), "/", 11};
    req.set(http::field::host, m_server);
    req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
    req.set(http::field::content_type, macros::CONTENT_TYPE_GZIP);
    req.body() = std::move(body);
    req.prepare_payload();

    http::write(m_socket, req, ec);
    if (ec)
    {
      log::ERROR<Dispatch>("Failed to send request: {}", ec.message());
      return;
    }

    // Read response from server
    beast::flat_buffer                buffer;
    http::response<http::string_body> res;
    http::read(m_socket, buffer, res, ec);

    if (ec)
    {
      log::ERROR<Dispatch>("Failed to read response: {}", ec.message());
      return;
    }

    // Extract and log Audio-ID separately
    const auto audio_id_it = res.find("Audio-ID");
    if (audio_id_it != res.end())
    {
      const auto AUDIO_ID = audio_id_it->value();
      log::INFO<Dispatch>("Parsed Audio-ID: {}", std::string(AUDIO_ID));
    }
    else
    {
      log::WARN<Dispatch>("Audio-ID not found in response headers.");
    }
  }

  void print_hierarchy()
  {
    log::INFO<log::NONE>("\n HLS Playlist Hierarchy:\n");
    std::cout << ">> " << m_playlistName << "\n";

    for (const auto& [playlist, segments] : m_refPlaylists)
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
