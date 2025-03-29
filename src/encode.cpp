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

#include "../include/encode.hpp"
#include "../include/registry.hpp"

/*
 * @NOTE:
 *
 * It is recommended by the FFmpeg Developers to use av_log()
 * instead of stdout hence if `--debug` is invoked, it will
 * provide with verbose AV LOGS regarding the HLS encoding process.
 *
 */

auto main(int argc, char* argv[]) -> int
{
  logger::init_logging();

  if (argc < 4)
  {
    LOG_ERROR << "Usage: " << argv[0]
              << " <input file> <output directory> <audio format> [--debug]";
    return 1;
  }

  bool debug_mode = false;
  for (int i = 4; i < argc; ++i)
  {
    if (strcmp(argv[i], "--debug") == 0)
    {
      debug_mode = true;
      break;
    }
  }

  if (debug_mode)
  {
    LOG_INFO << "-- Debug mode enabled: AV_LOG will output verbose logs.";
    av_log_set_level(AV_LOG_DEBUG);
  }
  else
  {
    av_log_set_level(AV_LOG_ERROR); // Only critical errors
  }

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
  std::vector<int> bitrates = {64, 128, 256};        // Example bitrates in kbps
  /* This is a godawful way of doing it will fix. */ //[TODO]: Fix this command line argument
                                                     // structure
  bool        use_flac   = (strcmp(argv[3], "flac") == 0);
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

  HLS_Encoder encoder;
  encoder.create_hls_segments(argv[1], bitrates, argv[2], use_flac);
  LOG_INFO << "Encoding seems to be complete.";

  AudioParser parser(argv[1]);
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
