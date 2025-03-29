#pragma once
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


extern "C"
{
#include <libavformat/avformat.h>
#include <libavutil/dict.h>
}

namespace libwavy::ffmpeg
{

class Metadata
{
public:
  auto fetchBitrate(const char* input_file) -> int
  {
    AVFormatContext*         fmt_ctx = nullptr;
    const AVDictionaryEntry* tag     = nullptr;
    int                      ret;

    if ((ret = avformat_open_input(&fmt_ctx, input_file, nullptr, nullptr)))
      return ret;

    if ((ret = avformat_find_stream_info(fmt_ctx, nullptr)) < 0)
    {
      av_log(nullptr, AV_LOG_ERROR, "Cannot find stream information\n");
      return ret;
    }

    if (fmt_ctx)
      return fmt_ctx->bit_rate;

    avformat_close_input(&fmt_ctx);
    return 0;
  }
};

} // namespace libwavy::ffmpeg
