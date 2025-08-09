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
#include <libwavy/server/prototypes.hpp>

#include <filesystem>
#include <fstream>
#include <utility>

namespace fs = std::filesystem;

using ServerDownload = libwavy::log::SERVER_DWNLD;

namespace libwavy::server::methods
{

class DownloadManager
{
public:
  // New constructor for direct use
  DownloadManager(std::string audio_id, const crow::request& req)
      : audioId(std::move(audio_id)), request(req)
  {
  }

  auto runDirect(const StorageOwnerID& owner_id, const StorageAudioID& filename) -> crow::response
  {
    const fs::path file_path =
      fs::path(macros::to_string(macros::SERVER_STORAGE_DIR)) / owner_id / audioId / filename;

    log::INFO<ServerDownload>(LogMode::Async, "Attempting to serve file: {}", file_path.string());

    if (!fs::exists(file_path))
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

    return res;
  }

private:
  std::string          audioId;
  const crow::request& request;

  auto detectMimeType(const std::string& filename) -> std::string
  {
    if (filename.ends_with(macros::PLAYLIST_EXT))
      return "application/vnd.apple.mpegurl";
    if (filename.ends_with(macros::TRANSPORT_STREAM_EXT))
      return "video/mp2t";
    return macros::to_string(macros::CONTENT_TYPE_OCTET_STREAM);
  }

  auto readFile(const std::string& path) -> std::string
  {
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs)
      return {};
    return {std::istreambuf_iterator<char>(ifs), std::istreambuf_iterator<char>()};
  }
};

} // namespace libwavy::server::methods
