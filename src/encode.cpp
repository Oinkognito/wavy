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

#include <cstdlib>
#include <libwavy/ffmpeg/hls/entry.hpp>
#include <libwavy/ffmpeg/misc/metadata.hpp>
#include <libwavy/ffmpeg/transcoder/entry.hpp>
#include <libwavy/registry/entry.hpp>

/*
 * @NOTE:
 *
 * It is recommended by the FFmpeg Developers to use av_log()
 * instead of stdout hence if `--debug` is invoked, it will
 * provide with verbose AV LOGS regarding the HLS encoding process.
 *
 */

constexpr const char* HELP_STR = " <input file> <output directory> <audio format> [--debug]";

void DBG_printBitrateVec(vector<int>& vec)
{
  LOG_DEBUG << "Printing Bitrate Vec...";
  for (const auto& i : vec)
    LOG_DEBUG << i;
  LOG_DEBUG << "Finished listing.";
}

auto exportTOMLFile(const char* filename, const std::string& output_dir, vector<int> found_bitrates)
  -> int
{
  libwavy::registry::RegisterAudio parser(filename, found_bitrates);
  if (!parser.parse())
  {
    std::cerr << "Failed to parse audio file.\n";
    return 1;
  }

  std::string outputTOMLFile = output_dir + "/" + macros::to_string(macros::METADATA_FILE);

  parser.exportToTOML(macros::to_string(outputTOMLFile));
  LOG_INFO << ENCODER_LOG << "TOML metadata exported to " << outputTOMLFile;

  return 0;
}

auto checkDebug(std::span<char*> args) -> bool
{
  constexpr std::string_view debug_flag = "--debug";

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

void DBG_logCheck(const bool& debug_mode)
{
  if (debug_mode)
  {
    LOG_INFO << ENCODER_LOG << "-- Debug mode enabled: AV_LOG will output verbose logs.";
    av_log_set_level(AV_LOG_DEBUG);
  }
  else
  {
    av_log_set_level(AV_LOG_ERROR); // Only critical errors
  }
}

auto main(int argc, char* argv[]) -> int
{
  logger::init_logging();

  if (argc < 4)
  {
    LOG_ERROR << "Usage: " << argv[0] << HELP_STR;
    return 1;
  }

  bool debug_mode = checkDebug(std::span(argv + 4, argc - 4));

  DBG_logCheck(debug_mode);

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
  bool use_flac = (strcmp(argv[3], "flac") == 0);    // This is to encode in lossless FLAC format
  std::string output_dir = std::string(argv[2]);

  if (fs::exists(output_dir))
  {
    LOG_WARNING << ENCODER_LOG << "Output directory exists, rewriting...";
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

  libwavy::hls::HLS_Segmenter seg;

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
    libwavy::ffmpeg::Metadata met;
    int                       bitrate = met.fetchBitrate(argv[1]);
    LOG_TRACE << ENCODER_LOG << "Found bitrate: " << bitrate;
    const std::string hls_seg_name = output_dir + "/segment.m3u8";
    LOG_TRACE << ENCODER_LOG << "Encoding HLS segments for FLAC in '" << hls_seg_name
              << "'. Skipping transcoding...";
    seg.createSegmentsFLAC(argv[1], hls_seg_name.c_str(),
                           bitrate); // This will also create the master playlist
    return exportTOMLFile(argv[1], output_dir,
                          {}); // just give empty array as we are not transcoding to diff bitrates
  }

  libwavy::ffmpeg::Metadata metadata;
  vector<int>               found_bitrates;
  const char*               output_file =
    "output.mp3"; // this will constantly get re-written it can stay constant
  for (const auto& i : bitrates)
  {
    libwavy::ffmpeg::Transcoder trns;
    LOG_INFO << ENCODER_LOG << "Encoding for bitrate: " << i * 1000;
    int result = trns.transcode_mp3(argv[1], output_file, i * 1000);
    if (result == 0)
    {
      LOG_INFO << ENCODER_LOG << "Transcoding seems to be succesful. Creating HLS segmentes in '"
               << output_dir << "'.";
      found_bitrates = seg.createSegments(output_file, output_dir.c_str());
    }
  }
  LOG_INFO
    << ENCODER_LOG
    << "Total encoding seems to be complete. Going ahead with creating <master playlist> ...";

  // For debug if you want to sanity check the resultant bitrates
  DBG_printBitrateVec(found_bitrates);

  seg.createMasterPlaylistMP3(output_dir, output_dir);

  return exportTOMLFile(argv[1], output_dir, found_bitrates);
}
