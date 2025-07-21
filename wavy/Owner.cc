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

#include <autogen/config.h>
#include <libwavy/common/macros.hpp>
#include <span>
#include <unordered_set>

#include "helpers/Dispatcher.hpp"
#include <libwavy/log-macros.hpp>
#include <libwavy/ffmpeg/hls/entry.hpp>
#include <libwavy/ffmpeg/misc/metadata.hpp>
#include <libwavy/ffmpeg/transcoder/entry.hpp>
#include <libwavy/registry/entry.hpp>
#include <libwavy/utils/cmd-line/parser.hpp>
#include <tbb/concurrent_vector.h>
#include <tbb/parallel_for_each.h>

/*
 * @NOTE:
 *
 * It is recommended by the FFmpeg Developers to use av_log()
 * instead of stdout hence if `--debug` is invoked, it will
 * provide with verbose AV LOGS regarding the HLS encoding and transcoding process.
 *
 */

namespace lwlog = libwavy::log;
using Owner = lwlog::OWNER;

constexpr const char* HELP_STR =
  " --inputFile=<input file> --outputDir=<output directory> "
  "--flacEncode=<true/false> [--avDbgLog] --serverIP=<server-ip> --nickname=<unique string>";

auto exportTOMLFile(const FileName& filename, const StorageOwnerID& nickname,
                    const Directory& output_dir, vector<int> found_bitrates) -> int
{
  libwavy::registry::RegisterAudio parser(filename, nickname, found_bitrates);
  if (!parser.parse())
  {
    lwlog::ERROR<Owner>("Failed to parse audio file.");
    return WAVY_RET_FAIL;
  }

  // This path is relative to where the binary is run!!
  RelPath outputTOMLFile = output_dir + "/" + macros::to_string(macros::METADATA_FILE);

  parser.exportToTOML(macros::to_string(outputTOMLFile));
  lwlog::INFO<Owner>("TOML metadata exported to '{}'", outputTOMLFile);

  return WAVY_RET_SUC;
}

void DBG_AVlogCheck(const bool& avdebug_mode)
{
  if (avdebug_mode)
  {
    lwlog::INFO<Owner>("-- AV Debug mode enabled: AV_LOG will output verbose logs.");
    av_log_set_level(AV_LOG_DEBUG);
  }
  else
  {
    av_log_set_level(AV_LOG_ERROR); // Only critical errors
  }
}

auto main(int argc, char* argv[]) -> int
{
  lwlog::init_logging();
  lwlog::set_log_level(libwavy::log::__INFO__);

  libwavy::utils::cmdline::CmdLineParser cmdLineParser(std::span<char* const>(argv, argc),
                                                       HELP_STR);

  bool                 avdebug_mode = cmdLineParser.get("avDbgLog") == "true" ? true : false;
  bool                 use_flac     = cmdLineParser.get("flacEncode") == "true"
                                        ? true
                                        : false; // This is to encode in lossless FLAC format
  const RelPath        input_file   = cmdLineParser.get("inputFile");
  const IPAddr         server       = cmdLineParser.get("serverIP");
  const StorageOwnerID nickname     = cmdLineParser.get("nickname");
  const Directory      output_dir   = cmdLineParser.get("outputDir");

  cmdLineParser.requireMinArgs(5, argc);

  libwavy::ffmpeg::Metadata met;
  int      entryBitrate = met.fetchBitrate(input_file.c_str());
  lwlog::INFO<Owner>("Entry input file '{}' with bitrate: {}", input_file, entryBitrate);

  DBG_AVlogCheck(avdebug_mode);

  /*
   * @NOTE
   *
   * Lossy audio permanently loses data. So let us say that a MP3 file has
   * a bitrate of 190kpbs, encoding it to any bitrate > 190 kbps will NOT
   * affect the decoded audio stream. In fact, it could technically make it worse.
   *
   * The bitrates vector will undergo a thorough refactor after ABR implementation.
   *
   * But this is to be noted that lossy audio can only be segmented into:
   *
   * BITRATES < BITRATE(AUDIOFILE)
   *
   * $Example:
   *
   * Audio file (MPEG-TS HLS segmented) -> 190 kbps bitrate.
   *
   * Possible bitrates vector could look like:
   *
   * `std::vector<int> bitrates = { 64, 128, 190 }`
   *
   */
  std::vector<int> bitrates = {64, 112, 128};        // Example bitrates in kbps
  /* This is a godawful way of doing it will fix. */ //[TODO]: Fix this command line argument
                                                     // structure

  if (fs::exists(output_dir))
  {
    lwlog::WARN<Owner>("Output directory exists, rewriting...");
    fs::remove_all(output_dir);
    fs::remove_all(macros::DISPATCH_ARCHIVE_REL_PATH);
  }

  if (fs::create_directory(output_dir))
  {
    lwlog::INFO<Owner>("Directory created successfully: '{}'", fs::absolute(output_dir).string());
  }
  else
  {
    lwlog::ERROR<Owner>("Failed to create directory: '{}'", output_dir);
    return WAVY_RET_FAIL;
  }

  libwavy::ffmpeg::hls::HLS_Segmenter seg;

  /*
   * @NOTE:
   *
   * When HLS segmenting for FLAC files, the remuxing is great and no 
   * problems when running this code.
   *
   * However, when checking the HLS segments or master playlists' metadata,
   * the bitrate calculation can be miscalculated!!
   *
   * Rest assured, the data is LOSSLESS and bitrate does NOT change!
   *
   */
  if (use_flac)
  {
    lwlog::INFO<Owner>("Encoding HLS segments for FLAC -> FLAC. Skipping transcoding...");
    seg.createSegmentsFLAC(input_file, output_dir, "hls_flac.m3u8",
                           entryBitrate); // This will also create the master playlist
    int ret =
      exportTOMLFile(input_file.c_str(), nickname, output_dir,
                     {}); // just give empty array as we are not transcoding to diff bitrates
    if (ret == 0)
    {
      return dispatch(server, nickname, output_dir);
    }

    lwlog::ERROR<Owner>("Failed to export metadata. Quiting dispatch JOB.");
    return WAVY_RET_FAIL;
  }

  // Use oneTBB parallelizing for faster transcoding times
  //
  // On average it is 2x faster than sequential transcoding

  WAVY__ASSERT(
    [&]()
    {
      for (const auto& i : bitrates)
        if (i % 2 != 0)
          return false;
      return true;
    }());

  tbb::concurrent_vector<int> concurrent_found_bitrates;

  tbb::parallel_for_each(
    bitrates.begin(), bitrates.end(),
    [&](int i)
    {
      RelPath output_file_i =
        output_dir + "/output_" + std::to_string(i) + macros::to_string(macros::MP3_FILE_EXT);
      libwavy::ffmpeg::Transcoder trns;
      lwlog::INFO<Owner>(LogMode::Async, "[Bitrate: {}] Starting transcoding job...", i);
      int result = trns.transcode_to_mp3(input_file.c_str(), output_file_i.c_str(), i * 1000);
      if (result == 0)
      {
        lwlog::INFO<Owner>(LogMode::Async, "[Bitrate: {}] Transcoding job went OK. Creating HLS Segments...", i);
        std::vector<int> res = seg.createSegments(output_file_i.c_str(), output_dir.c_str());
        for (int b : res)
          concurrent_found_bitrates.push_back(b);
      }
      else
      {
        lwlog::WARN<Owner>(LogMode::Async, "[Bitrate: {}] Transcoding Job failed.", i);
      }

      std::remove(output_file_i.c_str());
    });

  lwlog::INFO<Owner>("Total TRANSCODING + HLS segmenting JOB seems to be complete. Going ahead with creating <master playlist> ...");

  std::unordered_set<int> unique_set(
    concurrent_found_bitrates.begin(),
    concurrent_found_bitrates.end()); // to retain unique values (fallback to concurrent_vector)
  std::vector<int> found_bitrates(unique_set.begin(), unique_set.end());

  seg.createMasterPlaylistMP3(output_dir, output_dir);

  if (exportTOMLFile(input_file, nickname, output_dir, found_bitrates) > 0)
  {
    lwlog::ERROR<Owner>("Failed to export metadata to `metadata.toml`. Exiting...");
    return WAVY_RET_FAIL;
  }

  return dispatch(server, nickname, output_dir);
}
