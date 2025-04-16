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

#include <algorithm>
#include <autogen/config.h>
#include <libwavy/common/macros.hpp>
#include <unordered_set>

#include "helpers/dispatcher.hpp"
#include <cstdlib>
#include <libwavy/ffmpeg/hls/entry.hpp>
#include <libwavy/ffmpeg/misc/metadata.hpp>
#include <libwavy/ffmpeg/transcoder/entry.hpp>
#include <libwavy/registry/entry.hpp>
#include <tbb/concurrent_vector.h>
#include <tbb/parallel_for_each.h>

using namespace libwavy::ffmpeg;

/*
 * @NOTE:
 *
 * It is recommended by the FFmpeg Developers to use av_log()
 * instead of stdout hence if `--debug` is invoked, it will
 * provide with verbose AV LOGS regarding the HLS encoding and transcoding process.
 *
 */

constexpr const char* HELP_STR =
  " <input file> <output directory> <audio format> [--debug] <server-ip>";

auto exportTOMLFile(const char* filename, const std::string& output_dir, vector<int> found_bitrates)
  -> int
{
  libwavy::registry::RegisterAudio parser(filename, found_bitrates);
  if (!parser.parse())
  {
    LOG_ERROR << OWNER_LOG << "Failed to parse audio file.";
    return WAVY_RET_FAIL;
  }

  std::string outputTOMLFile = output_dir + "/" + macros::to_string(macros::METADATA_FILE);

  parser.exportToTOML(macros::to_string(outputTOMLFile));
  LOG_INFO << OWNER_LOG << "TOML metadata exported to " << outputTOMLFile;

  return WAVY_RET_SUC;
}

auto checkAVDebug(std::span<char*> args) -> bool
{
  constexpr std::string_view debug_flag = "--debug-libav";

#if __cpp_lib_ranges >= 201911L
  // C++20 and above: use ranges
  return std::ranges::any_of(args, [debug_flag](char* arg)
                             { return std::string_view(arg) == debug_flag; });
#else
  // Pre-C++20 fallback
  return std::any_of(args.begin(), args.end(),
                     [debug_flag](char* arg) { return std::string_view(arg) == debug_flag; });
#endif
}

auto checkFLACEncode(std::span<char*> args) -> bool
{
  constexpr std::string_view flac_enc_flag = "--flac";

#if __cpp_lib_ranges >= 201911L
  // C++20 and above: use ranges
  return std::ranges::any_of(args, [flac_enc_flag](char* arg)
                             { return std::string_view(arg) == flac_enc_flag; });
#else
  // Pre-C++20 fallback
  return std::any_of(args.begin(), args.end(),
                     [flac_enc_flag](char* arg) { return std::string_view(arg) == flac_enc_flag; });
#endif
}

void DBG_AVlogCheck(const bool& avdebug_mode)
{
  if (avdebug_mode)
  {
    LOG_INFO << OWNER_LOG << "-- AV Debug mode enabled: AV_LOG will output verbose logs.";
    av_log_set_level(AV_LOG_DEBUG);
  }
  else
  {
    av_log_set_level(AV_LOG_ERROR); // Only critical errors
  }
}

auto main(int argc, char* argv[]) -> int
{
  libwavy::log::init_logging();

  if (argc < 5)
  {
    LOG_ERROR << "Usage: " << argv[0] << HELP_STR;
    return WAVY_RET_FAIL;
  }

  bool avdebug_mode = checkAVDebug(std::span(argv + 4, argc - 4));
  bool use_flac =
    checkFLACEncode(std::span(argv + 3, argc - 3)); // This is to encode in lossless FLAC format

  const std::string server = argv[4];

  Metadata met;
  int      entryBitrate = met.fetchBitrate(argv[1]);
  LOG_INFO << OWNER_LOG << "Entry input file's bitrate: " << entryBitrate;

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
  const std::string output_dir = std::string(argv[2]);

  if (fs::exists(output_dir))
  {
    LOG_WARNING << OWNER_LOG << "Output directory exists, rewriting...";
    fs::remove_all(output_dir);
    fs::remove_all(macros::DISPATCH_ARCHIVE_REL_PATH);
  }

  if (fs::create_directory(output_dir))
  {
    LOG_INFO << "Directory created successfully: " << fs::absolute(output_dir);
  }
  else
  {
    LOG_ERROR << "Failed to create directory: " << output_dir;
    return 1;
  }

  hls::HLS_Segmenter seg;

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
    LOG_INFO << OWNER_LOG << "Encoding HLS segments for FLAC -> FLAC. Skipping transcoding...";
    seg.createSegmentsFLAC(argv[1], output_dir, "hls_flac.m3u8",
                           entryBitrate); // This will also create the master playlist
    return exportTOMLFile(argv[1], output_dir,
                          {}); // just give empty array as we are not transcoding to diff bitrates
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
      std::string output_file_i = output_dir + "/output_" + std::to_string(i) + ".mp3";
      Transcoder  trns;
      LOG_INFO << OWNER_LOG << "[Bitrate: " << i << "] Starting transcoding...";
      int result = trns.transcode_mp3(argv[1], output_file_i.c_str(), i * 1000);
      if (result == 0)
      {
        LOG_INFO << OWNER_LOG << "[Bitrate: " << i
                 << "] Transcoding OK. Creating HLS segments...";
        std::vector<int> res = seg.createSegments(output_file_i.c_str(), output_dir.c_str());
        for (int b : res)
          concurrent_found_bitrates.push_back(b);
      }
      else
      {
        LOG_WARNING << OWNER_LOG << "[Bitrate: " << i << "] Transcoding failed.";
      }

      std::remove(output_file_i.c_str());
    });
  LOG_INFO << OWNER_LOG
           << "Total TRANSCODING + HLS segmenting JOB seems to be complete. Going ahead with creating "
              "<master playlist> ...";

  std::unordered_set<int> unique_set(
    concurrent_found_bitrates.begin(),
    concurrent_found_bitrates.end()); // to retain unique values (fallback to concurrent_vector)
  std::vector<int> found_bitrates(unique_set.begin(), unique_set.end());

  seg.createMasterPlaylistMP3(output_dir, output_dir);

  if (exportTOMLFile(argv[1], output_dir, found_bitrates) > 0)
  {
    LOG_ERROR << OWNER_LOG << "Failed to export metadata to `metadata.toml`. Exiting...";
    return WAVY_RET_FAIL;
  }

  return dispatch(server, output_dir);
}
