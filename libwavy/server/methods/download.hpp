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

#include <boost/filesystem.hpp>
#include <fstream>
#include <utility>

namespace bfs = boost::filesystem;

using ServerDownload = libwavy::log::SERVER_DWNLD;

namespace libwavy::server::methods
{

class DownloadManager
{
public:
  DownloadManager(Metrics& metrics, StorageOwnerID owner_id, StorageAudioID audio_id,
                  const crow::request& req)
      : m_metrics(metrics), m_ownerID(std::move(owner_id)), m_audioID(std::move(audio_id)),
        m_request(req)
  {
  }

  auto runDirect(const AbsPath& filename) -> crow::response
  {
    RequestTimer timer(m_metrics);
    m_metrics.download_requests++;

    m_metrics.owners[m_ownerID].downloads++;

    const bfs::path file_path =
      bfs::path(macros::to_string(macros::SERVER_STORAGE_DIR)) / m_ownerID / m_audioID / filename;

    log::INFO<ServerDownload>(LogMode::Async, "Attempting to serve file: {}", file_path.string());

    if (!bfs::exists(file_path))
    {
      log::ERROR<ServerDownload>(LogMode::Async, "File not found: {}", file_path.string());
      return {404, "File not found."};
    }

    std::string content_type = detectMimeType(filename);

    crow::response res;
    res.code = 200;
    res.set_header("Server", "Wavy Server");
    res.set_header("Content-Type", content_type);
    res.body = readFile(file_path.string());
    res.set_header("Content-Length", std::to_string(res.body.size()));

    log::INFO<ServerDownload>(LogMode::Async, "Serving '{}' ({} bytes) [{}]", filename,
                              res.body.size(), content_type);

    guessDownloadSize(res, timer);

    return res;
  }

private:
  StorageOwnerID       m_ownerID;
  StorageAudioID       m_audioID;
  const crow::request& m_request;
  Metrics&             m_metrics;

  auto detectMimeType(const AbsPath& filename) -> std::string
  {
    if (filename.ends_with(macros::PLAYLIST_EXT))
      return "application/vnd.apple.mpegurl";
    if (filename.ends_with(macros::TRANSPORT_STREAM_EXT))
      return "video/mp2t";
    return macros::to_string(macros::CONTENT_TYPE_OCTET_STREAM);
  }

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
