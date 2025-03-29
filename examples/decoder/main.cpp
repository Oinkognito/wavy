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

#include "../../include/decode.hpp"
#include "../../include/libwavy-common/state.hpp"

auto main() -> int
{
  GlobalState gs;

  std::vector<std::string> ts_segments = gs.transport_segments;

  if (ts_segments.empty())
  {
    av_log(nullptr, AV_LOG_ERROR, "No transport stream segments provided\n");
    return 1;
  }

  MediaDecoder               decoder;
  std::vector<unsigned char> decoded_audio;
  if (!decoder.decode(gs.transport_segments, decoded_audio))
  {
    av_log(nullptr, AV_LOG_ERROR, "Decoding failed\n");
    return 1;
  }

  return 0;
}
