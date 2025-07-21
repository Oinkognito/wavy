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

#include <libwavy/common/state.hpp>
#include <libwavy/ffmpeg/decoder/entry.hpp>

auto main() -> int
{
  GlobalState gs;

  auto ts_segments = gs.getAllSegments();

  if (ts_segments.empty())
  {
    av_log(nullptr, AV_LOG_ERROR, "No transport stream segments provided\n");
    return 1;
  }

  libwavy::ffmpeg::MediaDecoder decoder;
  TotalDecodedAudioData         decoded_audio;
  if (!decoder.decode(ts_segments, decoded_audio))
  {
    av_log(nullptr, AV_LOG_ERROR, "Decoding failed\n");
    return 1;
  }

  return 0;
}
