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

#include <crow.h>
#include <libwavy/common/macros.hpp>
#include <libwavy/common/types.hpp>
#include <libwavy/log-macros.hpp>
#include <libwavy/server/metrics.hpp>
#include <libwavy/server/prototypes.hpp>
#include <libwavy/server/request-timer.hpp>

#include <fstream>
#include <utility>

namespace bfs = boost::filesystem;

using ServerDownload = libwavy::log::SERVER_DWNLD;

namespace libwavy::server::methods
{

inline auto detectStreamMIMEType(const AbsPath& filename) -> const std::string
{
  if (filename.ends_with(macros::PLAYLIST_EXT))
    return "application/vnd.apple.mpegurl";
  if (filename.ends_with(macros::TRANSPORT_STREAM_EXT))
    return "video/mp2t";
  return macros::to_string(macros::CONTENT_TYPE_OCTET_STREAM);
}

class DownloadManager
{
public:
  DownloadManager(Metrics& metrics, StorageOwnerID owner_id, StorageAudioID audio_id,
                  const crow::request& req, db::LMDBKV<AudioMetadataPlain>& kvStore)
      : m_metrics(metrics), m_ownerID(std::move(owner_id)), m_audioID(std::move(audio_id)),
        m_request(req), m_kvStore(kvStore)
  {
  }

  auto runDirect(const AbsPath& filename) -> crow::response
  {
    RequestTimer timer(m_metrics);
    m_metrics.download_requests++;
    m_metrics.owners[m_ownerID].downloads++;

    // Compose the key in the LMDB
    const db::Key key = db::make_kv_key(m_ownerID, m_audioID, filename);

    // Fetch the value from the KV store
    db::Value file_val = m_kvStore.get(key);
    if (file_val.empty())
    {
      log::ERROR<ServerDownload>(LogMode::Async, "File not found in KV store: {}", filename);
      return {404, "File not found."};
    }

    std::string content_type = detectStreamMIMEType(filename);

    // Convert vector<char> (db::Value) to std::string for response body
    std::string body = as::str(file_val);

    crow::response res;
    res.code = 200;
    res.set_header("Server", "Wavy Server");
    res.set_header("Content-Type", content_type);
    res.body = std::move(body);
    res.set_header("Content-Length", std::to_string(res.body.size()));

    log::INFO<ServerDownload>(LogMode::Async, "Serving '{}' ({} bytes) [{}]", filename,
                              res.body.size(), content_type);

    guessDownloadSize(res, timer);

    return res;
  }

  auto runStream(const AbsPath& filename, crow::response& res) -> void
  {
    RequestTimer timer(m_metrics);
    m_metrics.download_requests++;
    m_metrics.owners[m_ownerID].downloads++;

    // Compose the key for KV store
    const db::Key key = db::make_kv_key(m_ownerID, m_audioID, filename);

    // Fetch the value from LMDB
    db::Value file_val = m_kvStore.get(key);
    if (file_val.empty())
    {
      log::ERROR<ServerDownload>("File not found in KV store: {}", filename);
      res.code = 404;
      res.end("File not found");
      return;
    }

    // Set content type
    const std::string content_type = detectStreamMIMEType(filename);
    res.set_header("Content-Type", content_type);
    res.set_header("Transfer-Encoding", "chunked");

    // Helper lambda to write a chunk
    auto L_WriteChunk = [&res](const std::string& data)
    {
      if (!data.empty())
      {
        std::stringstream hex_size;
        hex_size << std::hex << data.size();
        res.write(hex_size.str() + CRLF);
        res.write(data + CRLF);
      }
    };

    constexpr size_t CHUNK            = 64 * 1024;
    size_t           total_bytes_sent = 0;

    // Stream from vector<char> in chunks
    const char* ptr       = file_val.data();
    size_t      remaining = file_val.size();

    while (remaining > 0)
    {
      size_t chunk_size = std::min(remaining, CHUNK);
      L_WriteChunk(std::string(ptr, chunk_size));

      ptr += chunk_size;
      remaining -= chunk_size;
      total_bytes_sent += chunk_size;

      log::DBG<ServerDownload>("Wrote {} bytes to response (total: {})", chunk_size,
                               total_bytes_sent);
    }

    // Final empty chunk
    res.write(std::format("0{}", CRLF2));

    log::INFO<ServerDownload>("Finished streaming {} bytes for '{}'", total_bytes_sent, filename);
    res.end();

    // Update metrics
    m_metrics.bytes_downloaded += total_bytes_sent;
    timer.mark_success();
  }

private:
  StorageOwnerID                  m_ownerID;
  StorageAudioID                  m_audioID;
  const crow::request&            m_request;
  Metrics&                        m_metrics;
  db::LMDBKV<AudioMetadataPlain>& m_kvStore;

  auto readFile(const AbsPath& path) -> std::string
  {
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs)
      return {};
    return {std::istreambuf_iterator<char>(ifs), std::istreambuf_iterator<char>()};
  }

  void guessDownloadSize(crow::response& response, RequestTimer& timer)
  {
    // Try to estimate downloaded bytes from response
    if (response.code == 200 && !response.body.empty())
    {
      m_metrics.bytes_downloaded += response.body.size();
      timer.mark_success();
    }
    else if (response.code == 404)
    {
      timer.mark_error_404();
    }
    else if (response.code >= 400 && response.code < 500)
    {
      timer.mark_error_400();
    }
    else if (response.code >= 500)
    {
      timer.mark_error_500();
    }
  }
};

} // namespace libwavy::server::methods
