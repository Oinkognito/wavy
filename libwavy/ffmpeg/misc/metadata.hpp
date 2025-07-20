#pragma once
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

extern "C"
{
#include <libavformat/avformat.h>
#include <libavutil/dict.h>
}

namespace libwavy::ffmpeg
{

/**
 * @class Metadata
 * @brief A class for extracting metadata from multimedia files using FFmpeg.
 */
class Metadata
{
public:
  /**
   * @brief Fetches the bitrate of the given multimedia file.
   *
   * This function opens the provided multimedia file using FFmpeg, extracts its
   * metadata, and returns the bitrate in bits per second (bps).
   *
   * @param input_file The path to the multimedia file.
   * @return The bitrate of the file in bps, or a negative value if an error occurs.
   *
   * @note If an error occurs while opening the file or extracting stream info,
   *       a negative error code will be returned.
   */
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
