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
#include <libwavy/ffmpeg/transcoder/entry.hpp>

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
  INIT_WAVY_LOGGER_ALL();
  // Register all codecs and formats (not needed in newer FFmpeg versions but included for compatibility)
  libwavy::ffmpeg::Transcoder trns;

  if (argc != 4)
  {
    lwlog::ERROR<_>("Usage: {} <input-file> <output-mp3-file> <bitrate-in-bits/sec>", argv[0]);
    lwlog::INFO<_>("Example: {} input.flac output.mp3 128000", argv[0]);
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
    lwlog::ERROR<_>("Bitrate must be a valid integer!");
    return 1;
  }

  if (bitrate <= 0)
  {
    lwlog::ERROR<_>("itrate must be positive! (>0)");
    return 1;
  }

  int result = trns.transcode_to_mp3(input_file, output_file, bitrate);
  return (result < WAVY_RET_SUC) ? WAVY_RET_FAIL : WAVY_RET_SUC;
}
