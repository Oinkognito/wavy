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

#include <algorithm>
#include <iostream>
#include <string>
extern "C"
{
#include <libavformat/avformat.h>
#include <libavutil/channel_layout.h>
#include <libavutil/dict.h>
#include <libavutil/samplefmt.h>
}
#include <libwavy/common/types.hpp>
#include <libwavy/toml/toml_generator.hpp>
#include <libwavy/toml/toml_parser.hpp>

namespace libwavy::registry
{

using namespace libwavy::Toml;

class RegisterAudio
{
public:
  RegisterAudio(AbsPath filePath, std::vector<int> bitrates)
      : filePath(std::move(filePath)), bitrates(std::move(bitrates))
  {
  }

  ~RegisterAudio()
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

    populateMetadata();
    return true;
  }

  void exportToTOML(const AbsPath& outputFile) const
  {
    TomlGenerator tomlGen;

    // Do NOT put this in global context to avoid confusion!!
    using namespace TomlKeys;

    tomlGen.addTableValue(Audio::Parser, Audio::Path, metadata.path);
    tomlGen.addTableValue(Audio::Parser, Audio::FileFormat, metadata.file_format);
    tomlGen.addTableValue(Audio::Parser, Audio::FileFormatLong, metadata.file_format_long);
    tomlGen.addTableValue(Audio::Parser, Audio::Duration, metadata.duration);
    tomlGen.addTableValue(Audio::Parser, Audio::Bitrate, metadata.bitrate);
    tomlGen.addTableArray(Audio::Parser, Audio::TranscodedRates, bitrates);

    tomlGen.addTableValue(Metadata::Root, Metadata::Title, metadata.title);
    tomlGen.addTableValue(Metadata::Root, Metadata::Artist, metadata.artist);
    tomlGen.addTableValue(Metadata::Root, Metadata::Album, metadata.album);
    tomlGen.addTableValue(Metadata::Root, Metadata::Track,
                          std::to_string(metadata.track.first) + "/" +
                            std::to_string(metadata.track.second));
    tomlGen.addTableValue(Metadata::Root, Metadata::Disc,
                          std::to_string(metadata.disc.first) + "/" +
                            std::to_string(metadata.disc.second));
    tomlGen.addTableValue(Metadata::Root, Metadata::Copyright, metadata.copyright);
    tomlGen.addTableValue(Metadata::Root, Metadata::Genre, metadata.genre);
    tomlGen.addTableValue(Metadata::Root, Metadata::Comment, metadata.comment);
    tomlGen.addTableValue(Metadata::Root, Metadata::AlbumArtist, metadata.album_artist);
    tomlGen.addTableValue(Metadata::Root, Metadata::TSRC, metadata.tsrc);
    tomlGen.addTableValue(Metadata::Root, Metadata::Encoder, metadata.encoder);
    tomlGen.addTableValue(Metadata::Root, Metadata::EncodedBy, metadata.encoded_by);
    tomlGen.addTableValue(Metadata::Root, Metadata::Date, metadata.date);

    saveStreamMetadataToToml(tomlGen, metadata.audio_stream, Stream::Stream0);
    saveStreamMetadataToToml(tomlGen, metadata.video_stream, Stream::Stream1);

    tomlGen.saveToFile(outputFile);
  }

  [[nodiscard]] auto getMetadata() const -> const AudioMetadata& { return metadata; }

private:
  std::string      filePath;
  AVFormatContext* fmt_ctx{};
  AudioMetadata    metadata;
  std::vector<int> bitrates;

  void populateMetadata()
  {
    metadata.path = filePath;

    using namespace TomlKeys;

    if (fmt_ctx->iformat)
    {
      metadata.file_format = fmt_ctx->iformat->name ? std::string(fmt_ctx->iformat->name) : "";
      metadata.file_format_long =
        fmt_ctx->iformat->long_name ? std::string(fmt_ctx->iformat->long_name) : "";
    }

    metadata.duration =
      (fmt_ctx->duration != AV_NOPTS_VALUE) ? fmt_ctx->duration / AV_TIME_BASE : -1;
    metadata.bitrate = (fmt_ctx->bit_rate > 0) ? fmt_ctx->bit_rate / 1000 : -1;

    const AVDictionaryEntry* tag = nullptr;
    while ((tag = av_dict_iterate(fmt_ctx->metadata, tag)))
    {
      std::string key   = tag->key;
      std::string value = tag->value;

      std::ranges::transform(key, key.begin(), [](unsigned char c) { return std::tolower(c); });

      if (key == Metadata::Title)
        metadata.title = value;
      else if (key == Metadata::Artist)
        metadata.artist = value;
      else if (key == Metadata::Album)
        metadata.album = value;
      else if (key == Metadata::Track)
        metadata.track = parseFraction(value);
      else if (key == Metadata::Disc)
        metadata.disc = parseFraction(value);
      else if (key == Metadata::Copyright)
        metadata.copyright = value;
      else if (key == Metadata::Genre)
        metadata.genre = value;
      else if (key == Metadata::Comment)
        metadata.comment = value;
      else if (key == Metadata::AlbumArtist)
        metadata.album_artist = value;
      else if (key == Metadata::TSRC)
        metadata.tsrc = value;
      else if (key == Metadata::Encoder)
        metadata.encoder = value;
      else if (key == Metadata::EncodedBy)
        metadata.encoded_by = value;
      else if (key == Metadata::Date)
        metadata.date = value;
    }

    // Extract streams
    for (unsigned i = 0; i < fmt_ctx->nb_streams; i++)
    {
      AVStream*          stream       = fmt_ctx->streams[i];
      AVCodecParameters* codec_params = stream->codecpar;

      StreamMetadata streamMetadata;

      if (codec_params->codec_id != AV_CODEC_ID_NONE)
        streamMetadata.codec = avcodec_get_name(codec_params->codec_id);

      streamMetadata.type = (codec_params->codec_type == AVMEDIA_TYPE_AUDIO)   ? "Audio"
                            : (codec_params->codec_type == AVMEDIA_TYPE_VIDEO) ? "Video"
                                                                               : "Unknown";

      if (codec_params->codec_type == AVMEDIA_TYPE_AUDIO)
      {
        streamMetadata.sample_rate = codec_params->sample_rate;
        streamMetadata.channels    = codec_params->ch_layout.nb_channels;
        streamMetadata.bitrate     = codec_params->bit_rate / 1000;

        if (codec_params->format != AV_SAMPLE_FMT_NONE)
          streamMetadata.sample_format =
            av_get_sample_fmt_name(static_cast<AVSampleFormat>(codec_params->format));

        char ch_layout_str[256];
        av_channel_layout_describe(&codec_params->ch_layout, ch_layout_str, sizeof(ch_layout_str));
        streamMetadata.channel_layout = ch_layout_str;

        metadata.audio_stream = streamMetadata;
      }
      else if (codec_params->codec_type == AVMEDIA_TYPE_VIDEO)
      {
        metadata.video_stream = streamMetadata;
      }
    }
  }

  static void saveStreamMetadataToToml(TomlGenerator& tomlGen, const StreamMetadata& stream,
                                       const std::string& parent)
  {

    using namespace TomlKeys;

    tomlGen.addTableValue(parent, Stream::Codec, stream.codec);
    tomlGen.addTableValue(parent, Stream::Type, stream.type);
    tomlGen.addTableValue(parent, Stream::SampleRate, stream.sample_rate);
    tomlGen.addTableValue(parent, Stream::Channels, stream.channels);
    tomlGen.addTableValue(parent, Stream::Bitrate, stream.bitrate);
    tomlGen.addTableValue(parent, Stream::SampleFormat, stream.sample_format);
    tomlGen.addTableValue(parent, Stream::ChannelLayout, stream.channel_layout);
  }
};

} // namespace libwavy::registry
