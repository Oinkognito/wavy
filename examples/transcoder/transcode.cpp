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

#include "../../include/libwavy-ffmpeg/transcoder/entry.hpp"

/*
 * This is a universal transcoder that works in the following formats:
 *
 * flac (16,32 bit) ==> mp3 transcoding 
 * mp3 ==> mp3 transcoding 
 *
 * wav to mp3 is also possible I believe if we use FLAC++ API to decode WAV to FLAC first
 *
 */

auto main(int argc, char* argv[]) -> int
{
  logger::init_logging();
  // Register all codecs and formats (not needed in newer FFmpeg versions but included for compatibility)
  libwavy::ffmpeg::Transcoder trns;

  if (argc != 4)
  {
    LOG_ERROR << "Usage: " << argv[0] << " <input-file> <output-mp3-file> <bitrate-in-bits/sec>";
    LOG_INFO << "Example: " << argv[0] << " input.flac output.mp3 128000";
    return 1;
  }

  const char* input_file  = argv[1];
  const char* output_file = argv[2];
  int         bitrate;

  try
  {
    bitrate = std::stoi(argv[3]);
  }
  catch (const std::exception& e)
  {
    LOG_ERROR << "Error: Bitrate must be a valid integer";
    return 1;
  }

  if (bitrate <= 0)
  {
    LOG_ERROR << "Error: Bitrate must be positive";
    return 1;
  }

  int result = trns.transcode_mp3(input_file, output_file, bitrate);
  return (result < 0) ? 1 : 0;
}
