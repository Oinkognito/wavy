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

#include <libwavy/common/macros.hpp>
#include <libwavy/common/types.hpp>
#include <libwavy/parser/entry.hpp>
#include <libwavy/utils/io/file/entry.hpp>

namespace bfs = boost::filesystem;
using namespace libwavy::hls::parser;
using StdFileUtil = libwavy::utils::FileUtil<PlaylistData>;

void printError(const std::string& bin)
{
  LOG_ERROR << "Usage: " << bin << " <playlist.m3u8> [master_playlist=0|1] [use_string_parser=0|1]";
  LOG_ERROR << "0 -> False; 1 -> True";
  LOG_INFO << "Examples:";
  LOG_INFO << "1. " << bin << " index.m3u8 1 0             [Parse as MASTER playlist from file]";
  LOG_INFO << "2. " << bin << " $(cat hls_mp3_64.m3u8) 0 1 [Parse as MEDIA playlist from string]";
}

// If we want the string parser (from stdin), try to read the PlaylistData, else just pass over the path
auto readContentIfRequired(const RelPath& path, bool use_string_parser) -> PlaylistData
{
  return use_string_parser ? path : StdFileUtil::readFile(path);
}

auto main(int argc, char* argv[]) -> int
{
  INIT_WAVY_LOGGER_ALL();
  libwavy::log::set_log_level(libwavy::log::__INFO__);

  if (argc < 4)
  {
    printError(argv[0]);
    return WAVY_RET_FAIL;
  }

  const RelPath playlist_path      = argv[1];
  const bool    is_master_playlist = (argc > 2) && std::stoi(argv[2]) > 0;
  const bool    use_string_parser  = (argc > 3) && std::stoi(argv[3]) > 0;

  lwlog::INFO<_>("Parsing: {} using {} parser...", playlist_path,
                 (use_string_parser ? "string" : "file path"));
  lwlog::INFO<_>("Job to parse MASTER Playlist: {}", (is_master_playlist ? "TRUE" : "FALSE"));

  try
  {
    const auto base_dir = bfs::path(playlist_path).parent_path().string();

    if (is_master_playlist)
    {
      const auto content = readContentIfRequired(playlist_path, use_string_parser);
      auto       master  = use_string_parser ? M3U8Parser::parseMasterPlaylist(content)
                                             : M3U8Parser::parseMasterPlaylist(content, base_dir);

      lwlog::INFO<_>("Parsed master playlist with {} variants.", master.variants.size());

      for (const auto& variant : master.variants)
      {
        const auto& uri      = variant.uri;
        const auto  mediaDir = bfs::path(uri).parent_path().string();
        const auto  mediaStr = readContentIfRequired(uri, use_string_parser);

        auto media = use_string_parser
                       ? M3U8Parser::parseMediaPlaylist(mediaStr, variant.bitrate, mediaDir)
                       : M3U8Parser::parseMediaPlaylist(mediaStr, variant.bitrate, mediaDir);

        lwlog::INFO<_>("Parsed media playlist @bitrate: {}", variant.bitrate);
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

      lwlog::INFO<_>("Parsed media playlist successfully!!");
      printAST(media);
    }

    lwlog::INFO<_>("All playlists parsed successfully");
    return WAVY_RET_SUC;
  }
  catch (const std::exception& e)
  {
    lwlog::ERROR<_>("Parsing failed: {}", e.what());
    return WAVY_RET_FAIL;
  }
}
