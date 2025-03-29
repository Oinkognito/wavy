#pragma once

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
