#include <iostream>
#include <fstream>
#include <string>
extern "C"
{
#include <libavformat/avformat.h>
#include <libavutil/dict.h>
#include <libavutil/channel_layout.h>
#include <libavutil/samplefmt.h>
}
#include "toml/toml_generator.hpp"

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

  void exportToTOML(const std::string &outputFile) const
  {
      TomlGenerator tomlGen;
      
      tomlGen.addTableValue("audio_parser", "file", filePath);

      if (fmt_ctx->iformat)
      {
          tomlGen.addTableValue("audio_parser", "file.format", std::string(fmt_ctx->iformat->name));
          if (fmt_ctx->iformat->long_name)
          {
              tomlGen.addTableValue("audio_parser", "file.format_long", std::string(fmt_ctx->iformat->long_name));
          }
      }

      if (fmt_ctx->duration != AV_NOPTS_VALUE)
      {
          tomlGen.addTableValue("audio_parser", "file.duration", static_cast<int>(fmt_ctx->duration / AV_TIME_BASE));
      }

      if (fmt_ctx->bit_rate > 0)
      {
          tomlGen.addTableValue("audio_parser", "file.bitrate", static_cast<int>(fmt_ctx->bit_rate / 1000));
      }

      // Metadata
      const AVDictionaryEntry *tag = nullptr;
      while ((tag = av_dict_iterate(fmt_ctx->metadata, tag)))
      {
          tomlGen.addTableValue("audio_parser.metadata", tag->key, std::string(tag->value));
      }

      // Streams
      for (unsigned i = 0; i < fmt_ctx->nb_streams; i++)
      {
          AVStream *stream = fmt_ctx->streams[i];
          AVCodecParameters *codec_params = stream->codecpar;

          std::string streamPath = "audio_parser.stream_" + std::to_string(i);

          if (codec_params->codec_id != AV_CODEC_ID_NONE)
          {
              tomlGen.addTableValue(streamPath, "codec", std::string(avcodec_get_name(codec_params->codec_id)));
          }

          switch (codec_params->codec_type)
          {
          case AVMEDIA_TYPE_AUDIO:
              tomlGen.addTableValue(streamPath, "type", "Audio");
              break;
          case AVMEDIA_TYPE_VIDEO:
              tomlGen.addTableValue(streamPath, "type", "Video");
              break;
          case AVMEDIA_TYPE_SUBTITLE:
              tomlGen.addTableValue(streamPath, "type", "Subtitle");
              break;
          default:
              tomlGen.addTableValue(streamPath, "type", "Unknown");
              break;
          }

          if (codec_params->codec_type == AVMEDIA_TYPE_AUDIO)
          {
              tomlGen.addTableValue(streamPath, "sample_rate", codec_params->sample_rate);
              tomlGen.addTableValue(streamPath, "channels", codec_params->ch_layout.nb_channels);
              tomlGen.addTableValue(streamPath, "bitrate", static_cast<int>(codec_params->bit_rate / 1000));

              if (codec_params->format != AV_SAMPLE_FMT_NONE)
              {
                  tomlGen.addTableValue(streamPath, "sample_format", std::string(av_get_sample_fmt_name(static_cast<AVSampleFormat>(codec_params->format))));
              }

              char ch_layout_str[256];
              av_channel_layout_describe(&codec_params->ch_layout, ch_layout_str, sizeof(ch_layout_str));
              tomlGen.addTableValue(streamPath, "channel_layout", std::string(ch_layout_str));
          }
      }

      tomlGen.saveToFile(outputFile);
  }

private:
  std::string filePath;
  AVFormatContext *fmt_ctx{};
};
