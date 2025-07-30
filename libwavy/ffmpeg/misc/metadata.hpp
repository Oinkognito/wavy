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

#include <libwavy/common/api/entry.hpp>
#include <libwavy/common/types.hpp>
#include <string>

namespace libwavy::ffmpeg
{

/**
 * @class Metadata
 * @brief A class for extracting metadata from multimedia files using FFmpeg.
 */
class WAVY_API Metadata
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
  auto fetchBitrate(CStrAbsPath input_file) -> int
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

  auto getAudioFormat(CStrAbsPath input_file) -> std::string
  {
    AVFormatContext* fmt_ctx = nullptr;
    int              ret;

    if ((ret = avformat_open_input(&fmt_ctx, input_file, nullptr, nullptr)) < 0)
      return "error: open failed";

    if ((ret = avformat_find_stream_info(fmt_ctx, nullptr)) < 0)
    {
      avformat_close_input(&fmt_ctx);
      return "error: no stream info";
    }

    // Find the first audio stream
    for (unsigned int i = 0; i < fmt_ctx->nb_streams; ++i)
    {
      AVStream* stream = fmt_ctx->streams[i];
      if (stream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
      {
        const AVCodec* codec = avcodec_find_decoder(stream->codecpar->codec_id);
        if (codec)
        {
          std::string codec_name = codec->name;
          avformat_close_input(&fmt_ctx);
          return codec_name;
        }
        else
        {
          avformat_close_input(&fmt_ctx);
          return "unknown_codec";
        }
      }
    }

    avformat_close_input(&fmt_ctx);
    return "no_audio_stream";
  }
};

} // namespace libwavy::ffmpeg
