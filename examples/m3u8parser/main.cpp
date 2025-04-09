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

#include <libwavy/common/macros.hpp>
#include <libwavy/logger.hpp>
#include <libwavy/parser/entry.hpp>
#include <libwavy/utils/io/file/entry.hpp>

namespace fs = boost::filesystem;
using namespace libwavy::hls::parser;
using StdFileUtil = libwavy::util::FileUtil<std::string>;

void printError(const std::string& bin)
{
  LOG_ERROR << "Usage: " << bin << " <playlist.m3u8> [master_playlist=0|1] [use_string_parser=0|1]";
  LOG_ERROR << "0 -> False; 1 -> True";
  LOG_INFO << "Examples:";
  LOG_INFO << "1. " << bin << " index.m3u8 1 0             [Parse as MASTER playlist from file]";
  LOG_INFO << "2. " << bin << " $(cat hls_mp3_64.m3u8) 0 1 [Parse as MEDIA playlist from string]";
}

auto readContentIfRequired(const std::string& path, bool use_string_parser) -> std::string
{
  return use_string_parser ? path : StdFileUtil::readFile(path);
}

auto main(int argc, char* argv[]) -> int
{
  libwavy::log::init_logging();
  libwavy::log::set_log_level(libwavy::log::INFO);

  if (argc < 4)
  {
    printError(argv[0]);
    return WAVY_RET_FAIL;
  }

  const std::string playlist_path      = argv[1];
  const bool        is_master_playlist = (argc > 2) && std::stoi(argv[2]) > 0;
  const bool        use_string_parser  = (argc > 3) && std::stoi(argv[3]) > 0;

  LOG_INFO << "Parsing: " << playlist_path << " using "
           << (use_string_parser ? "string" : "file path") << " parser";
  LOG_INFO << "Job to parse MASTER Playlist: " << (is_master_playlist ? "TRUE" : "FALSE");

  try
  {
    const auto base_dir = fs::path(playlist_path).parent_path().string();

    if (is_master_playlist)
    {
      const auto content = readContentIfRequired(playlist_path, use_string_parser);
      auto       master  = use_string_parser ? M3U8Parser::parseMasterPlaylist(content)
                                             : M3U8Parser::parseMasterPlaylist(content, base_dir);

      LOG_INFO << "Parsed master playlist with " << master.variants.size() << " variants";

      for (const auto& variant : master.variants)
      {
        const auto& uri      = variant.uri;
        const auto  mediaDir = fs::path(uri).parent_path().string();
        const auto  mediaStr = readContentIfRequired(uri, use_string_parser);

        auto media = use_string_parser
                       ? M3U8Parser::parseMediaPlaylist(mediaStr, variant.bitrate, mediaDir)
                       : M3U8Parser::parseMediaPlaylist(mediaStr, variant.bitrate, mediaDir);

        LOG_INFO << "Parsed media playlist @ bitrate " << variant.bitrate;
        printAST(media);
        master.media_playlists[variant.bitrate] = std::move(media);
      }

      printAST(master);
    }
    else
    {
      const auto content = readContentIfRequired(playlist_path, use_string_parser);
      const auto media   = use_string_parser
                             ? M3U8Parser::parseMediaPlaylist(content, /*bitrate=*/0, base_dir)
                             : M3U8Parser::parseMediaPlaylist(content, /*bitrate=*/0, base_dir);

      LOG_INFO << "Parsed media playlist successfully";
      printAST(media);
    }

    LOG_INFO << "All playlists parsed successfully";
    return WAVY_RET_SUC;
  }
  catch (const std::exception& e)
  {
    LOG_ERROR << "Parsing failed: " << e.what();
    return WAVY_RET_FAIL;
  }
}
