#include <fstream>
#include <iostream>
#include <string>
extern "C"
{
#include <libavformat/avformat.h>
#include <libavutil/channel_layout.h>
#include <libavutil/dict.h>
#include <libavutil/samplefmt.h>
}
#include "toml/toml_generator.hpp"
#include "toml/toml_parser.hpp"

class AudioParser
{
public:
  AudioParser(std::string filePath) : filePath(std::move(filePath)) {}

  ~AudioParser()
  {
    if (fmt_ctx)
    {
      avformat_close_input(&fmt_ctx);
    }
  }

  auto parse() -> bool
  {
    int ret;

    if ((ret = avformat_open_input(&fmt_ctx, filePath.c_str(), nullptr, nullptr)) < 0)
    {
      std::cerr << "Error opening input file: " << filePath << std::endl;
      return false;
    }

    if ((ret = avformat_find_stream_info(fmt_ctx, nullptr)) < 0)
    {
      std::cerr << "Cannot find stream information\n";
      avformat_close_input(&fmt_ctx);
      return false;
    }

    return true;
  }

  void exportToTOML(const std::string& outputFile) const
  {
    TomlGenerator tomlGen;

    tomlGen.addTableValue(PARENT_AUDIO_PARSER, PARENT_AUDIO_FIELD_PATH, filePath);

    if (fmt_ctx->iformat)
    {
      tomlGen.addTableValue(PARENT_AUDIO_PARSER, PARENT_AUDIO_FIELD_FILE_FORMAT,
                            std::string(fmt_ctx->iformat->name));
      if (fmt_ctx->iformat->long_name)
      {
        tomlGen.addTableValue(PARENT_AUDIO_PARSER, PARENT_AUDIO_FIELD_FILE_FORMAT_LONG,
                              std::string(fmt_ctx->iformat->long_name));
      }
    }

    if (fmt_ctx->duration != AV_NOPTS_VALUE)
    {
      tomlGen.addTableValue(PARENT_AUDIO_PARSER, PARENT_AUDIO_FIELD_DURATION,
                            static_cast<int>(fmt_ctx->duration / AV_TIME_BASE));
    }

    if (fmt_ctx->bit_rate > 0)
    {
      tomlGen.addTableValue(PARENT_AUDIO_PARSER, PARENT_AUDIO_FIELD_BITRATE,
                            static_cast<int>(fmt_ctx->bit_rate / 1000));
    }

    // Metadata
    const AVDictionaryEntry* tag = nullptr;
    while ((tag = av_dict_iterate(fmt_ctx->metadata, tag)))
    {
      tomlGen.addTableValue(PARENT_METADATA, tag->key, std::string(tag->value));
    }

    // Streams
    for (unsigned i = 0; i < fmt_ctx->nb_streams; i++)
    {
      AVStream*          stream       = fmt_ctx->streams[i];
      AVCodecParameters* codec_params = stream->codecpar;

      std::string streamPath = (i == 0) ? PARENT_STREAM_0 : PARENT_STREAM_1;

      if (codec_params->codec_id != AV_CODEC_ID_NONE)
      {
        tomlGen.addTableValue(streamPath, PARENT_STREAM_FIELD_CODEC,
                              std::string(avcodec_get_name(codec_params->codec_id)));
      }

      switch (codec_params->codec_type)
      {
        case AVMEDIA_TYPE_AUDIO:
          tomlGen.addTableValue(streamPath, PARENT_STREAM_FIELD_TYPE, "Audio");
          break;
        case AVMEDIA_TYPE_VIDEO:
          tomlGen.addTableValue(streamPath, PARENT_STREAM_FIELD_TYPE, "Video");
          break;
        case AVMEDIA_TYPE_SUBTITLE:
          tomlGen.addTableValue(streamPath, PARENT_STREAM_FIELD_TYPE, "Subtitle");
          break;
        default:
          tomlGen.addTableValue(streamPath, PARENT_STREAM_FIELD_TYPE, "Unknown");
          break;
      }

      if (codec_params->codec_type == AVMEDIA_TYPE_AUDIO)
      {
        tomlGen.addTableValue(streamPath, PARENT_STREAM_FIELD_SAMPLE_RATE,
                              codec_params->sample_rate);
        tomlGen.addTableValue(streamPath, PARENT_STREAM_FIELD_CHANNELS,
                              codec_params->ch_layout.nb_channels);
        tomlGen.addTableValue(streamPath, PARENT_STREAM_FIELD_BITRATE,
                              static_cast<int>(codec_params->bit_rate / 1000));

        if (codec_params->format != AV_SAMPLE_FMT_NONE)
        {
          tomlGen.addTableValue(
            streamPath, PARENT_STREAM_FIELD_SAMPLE_FORMAT,
            std::string(av_get_sample_fmt_name(static_cast<AVSampleFormat>(codec_params->format))));
        }

        char ch_layout_str[256];
        av_channel_layout_describe(&codec_params->ch_layout, ch_layout_str, sizeof(ch_layout_str));
        tomlGen.addTableValue(streamPath, PARENT_STREAM_FIELD_CHANNEL_LAYOUT,
                              std::string(ch_layout_str));
      }
    }

    tomlGen.saveToFile(outputFile);
  }

private:
  std::string      filePath;
  AVFormatContext* fmt_ctx{};
};
