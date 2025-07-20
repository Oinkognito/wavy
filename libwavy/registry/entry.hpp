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

class RegisterAudio
{
public:
  RegisterAudio(AbsPath filePath, StorageOwnerID nickname, std::vector<int> bitrates)
      : m_filePath(std::move(filePath)), m_nickname(std::move(nickname)),
        m_bitrates(std::move(bitrates))
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

    if ((ret = avformat_open_input(&fmt_ctx, m_filePath.c_str(), nullptr, nullptr)) < 0)
    {
      std::cerr << "Error opening input file: " << m_filePath << std::endl;
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
    Toml::TomlGenerator tomlGen;

    // Do NOT put this in global context to avoid confusion!!
    using namespace TomlKeys;

    tomlGen.addTableValue(Owner::OwnerID, Owner::Nickname, m_nickname);

    tomlGen.addTableValue(Audio::Parser, Audio::Path, m_metadata.path);
    tomlGen.addTableValue(Audio::Parser, Audio::FileFormat, m_metadata.file_format);
    tomlGen.addTableValue(Audio::Parser, Audio::FileFormatLong, m_metadata.file_format_long);
    tomlGen.addTableValue(Audio::Parser, Audio::Duration, m_metadata.duration);
    tomlGen.addTableValue(Audio::Parser, Audio::Bitrate, m_metadata.bitrate);
    tomlGen.addTableArray(Audio::Parser, Audio::TranscodedRates, m_bitrates);

    tomlGen.addTableValue(Metadata::Root, Metadata::Title, m_metadata.title);
    tomlGen.addTableValue(Metadata::Root, Metadata::Artist, m_metadata.artist);
    tomlGen.addTableValue(Metadata::Root, Metadata::Album, m_metadata.album);
    tomlGen.addTableValue(Metadata::Root, Metadata::Track,
                          std::to_string(m_metadata.track.first) + "/" +
                            std::to_string(m_metadata.track.second));
    tomlGen.addTableValue(Metadata::Root, Metadata::Disc,
                          std::to_string(m_metadata.disc.first) + "/" +
                            std::to_string(m_metadata.disc.second));
    tomlGen.addTableValue(Metadata::Root, Metadata::Copyright, m_metadata.copyright);
    tomlGen.addTableValue(Metadata::Root, Metadata::Genre, m_metadata.genre);
    tomlGen.addTableValue(Metadata::Root, Metadata::Comment, m_metadata.comment);
    tomlGen.addTableValue(Metadata::Root, Metadata::AlbumArtist, m_metadata.album_artist);
    tomlGen.addTableValue(Metadata::Root, Metadata::TSRC, m_metadata.tsrc);
    tomlGen.addTableValue(Metadata::Root, Metadata::Encoder, m_metadata.encoder);
    tomlGen.addTableValue(Metadata::Root, Metadata::EncodedBy, m_metadata.encoded_by);
    tomlGen.addTableValue(Metadata::Root, Metadata::Date, m_metadata.date);

    saveStreamMetadataToToml(tomlGen, m_metadata.audio_stream, Stream::Stream0);
    saveStreamMetadataToToml(tomlGen, m_metadata.video_stream, Stream::Stream1);

    tomlGen.saveToFile(outputFile);
  }

  [[nodiscard]] auto getMetadata() const -> const AudioMetadata& { return m_metadata; }

private:
  RelPath          m_filePath;
  AVFormatContext* fmt_ctx{};
  AudioMetadata    m_metadata;
  std::vector<int> m_bitrates;
  StorageOwnerID   m_nickname;

  void populateMetadata()
  {
    m_metadata.path = m_filePath;

    using namespace TomlKeys;

    if (fmt_ctx->iformat)
    {
      m_metadata.file_format = fmt_ctx->iformat->name ? std::string(fmt_ctx->iformat->name) : "";
      m_metadata.file_format_long =
        fmt_ctx->iformat->long_name ? std::string(fmt_ctx->iformat->long_name) : "";
    }

    m_metadata.duration =
      (fmt_ctx->duration != AV_NOPTS_VALUE) ? fmt_ctx->duration / AV_TIME_BASE : -1;
    m_metadata.bitrate = (fmt_ctx->bit_rate > 0) ? fmt_ctx->bit_rate / 1000 : -1;

    const AVDictionaryEntry* tag = nullptr;
    while ((tag = av_dict_iterate(fmt_ctx->metadata, tag)))
    {
      std::string key   = tag->key;
      std::string value = tag->value;

      std::ranges::transform(key, key.begin(), [](unsigned char c) { return std::tolower(c); });

      if (key == Metadata::Title)
        m_metadata.title = value;
      else if (key == Metadata::Artist)
        m_metadata.artist = value;
      else if (key == Metadata::Album)
        m_metadata.album = value;
      else if (key == Metadata::Track)
        m_metadata.track = parseFraction(value);
      else if (key == Metadata::Disc)
        m_metadata.disc = parseFraction(value);
      else if (key == Metadata::Copyright)
        m_metadata.copyright = value;
      else if (key == Metadata::Genre)
        m_metadata.genre = value;
      else if (key == Metadata::Comment)
        m_metadata.comment = value;
      else if (key == Metadata::AlbumArtist)
        m_metadata.album_artist = value;
      else if (key == Metadata::TSRC)
        m_metadata.tsrc = value;
      else if (key == Metadata::Encoder)
        m_metadata.encoder = value;
      else if (key == Metadata::EncodedBy)
        m_metadata.encoded_by = value;
      else if (key == Metadata::Date)
        m_metadata.date = value;
    }

    // Extract streams
    for (AudioStreamIdxIter i = 0; i < fmt_ctx->nb_streams; i++)
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

        m_metadata.audio_stream = streamMetadata;
      }
      else if (codec_params->codec_type == AVMEDIA_TYPE_VIDEO)
      {
        m_metadata.video_stream = streamMetadata;
      }
    }
  }

  static void saveStreamMetadataToToml(Toml::TomlGenerator& tomlGen, const StreamMetadata& stream,
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
