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

    // Fill the AudioMetadata structure
    populateMetadata();
    return true;
  }

  void exportToTOML(const std::string& outputFile) const
  {
    TomlGenerator tomlGen;

    tomlGen.addTableValue(PARENT_AUDIO_PARSER, PARENT_AUDIO_FIELD_PATH, metadata.path);
    tomlGen.addTableValue(PARENT_AUDIO_PARSER, PARENT_AUDIO_FIELD_FILE_FORMAT,
                          metadata.file_format);
    tomlGen.addTableValue(PARENT_AUDIO_PARSER, PARENT_AUDIO_FIELD_FILE_FORMAT_LONG,
                          metadata.file_format_long);
    tomlGen.addTableValue(PARENT_AUDIO_PARSER, PARENT_AUDIO_FIELD_DURATION, metadata.duration);
    tomlGen.addTableValue(PARENT_AUDIO_PARSER, PARENT_AUDIO_FIELD_BITRATE, metadata.bitrate);

    // Metadata fields
    tomlGen.addTableValue(PARENT_METADATA, PARENT_METADATA_FIELD_TITLE, metadata.title);
    tomlGen.addTableValue(PARENT_METADATA, PARENT_METADATA_FIELD_ARTIST, metadata.artist);
    tomlGen.addTableValue(PARENT_METADATA, PARENT_METADATA_FIELD_ALBUM, metadata.album);
    tomlGen.addTableValue(PARENT_METADATA, PARENT_METADATA_FIELD_TRACK,
                          std::to_string(metadata.track.first) + "/" +
                            std::to_string(metadata.track.second));
    tomlGen.addTableValue(PARENT_METADATA, PARENT_METADATA_FIELD_DISC,
                          std::to_string(metadata.disc.first) + "/" +
                            std::to_string(metadata.disc.second));
    tomlGen.addTableValue(PARENT_METADATA, PARENT_METADATA_FIELD_COPYRIGHT, metadata.copyright);
    tomlGen.addTableValue(PARENT_METADATA, PARENT_METADATA_FIELD_GENRE, metadata.genre);
    tomlGen.addTableValue(PARENT_METADATA, PARENT_METADATA_FIELD_COMMENT, metadata.comment);
    tomlGen.addTableValue(PARENT_METADATA, PARENT_METADATA_FIELD_ALBUM_ARTIST,
                          metadata.album_artist);
    tomlGen.addTableValue(PARENT_METADATA, PARENT_METADATA_FIELD_TSRC, metadata.tsrc);
    tomlGen.addTableValue(PARENT_METADATA, PARENT_METADATA_FIELD_ENCODER, metadata.encoder);
    tomlGen.addTableValue(PARENT_METADATA, PARENT_METADATA_FIELD_ENCODED_BY, metadata.encoded_by);
    tomlGen.addTableValue(PARENT_METADATA, PARENT_METADATA_FIELD_DATE, metadata.date);

    // Stream data
    saveStreamMetadataToToml(tomlGen, metadata.audio_stream, PARENT_STREAM_0);
    saveStreamMetadataToToml(tomlGen, metadata.video_stream, PARENT_STREAM_1);

    tomlGen.saveToFile(outputFile);
  }

  [[nodiscard]] auto getMetadata() const -> const AudioMetadata& { return metadata; }

private:
  std::string      filePath;
  AVFormatContext* fmt_ctx{};
  AudioMetadata    metadata;

  void populateMetadata()
  {
    metadata.path = filePath;

    if (fmt_ctx->iformat)
    {
      metadata.file_format = fmt_ctx->iformat->name ? std::string(fmt_ctx->iformat->name) : "";
      metadata.file_format_long =
        fmt_ctx->iformat->long_name ? std::string(fmt_ctx->iformat->long_name) : "";
    }

    metadata.duration =
      (fmt_ctx->duration != AV_NOPTS_VALUE) ? fmt_ctx->duration / AV_TIME_BASE : -1;
    metadata.bitrate = (fmt_ctx->bit_rate > 0) ? fmt_ctx->bit_rate / 1000 : -1;

    // Extract metadata
    const AVDictionaryEntry* tag = nullptr;
    while ((tag = av_dict_iterate(fmt_ctx->metadata, tag)))
    {
      std::string key   = tag->key;
      std::string value = tag->value;

      std::ranges::transform(key, key.begin(), [](unsigned char c) { return std::tolower(c); });

      if (key == PARENT_METADATA_FIELD_TITLE)
        metadata.title = value;
      else if (key == PARENT_METADATA_FIELD_ARTIST)
        metadata.artist = value;
      else if (key == PARENT_METADATA_FIELD_ALBUM)
        metadata.album = value;
      else if (key == PARENT_METADATA_FIELD_TRACK)
        metadata.track = parseFraction(value);
      else if (key == PARENT_METADATA_FIELD_DISC)
        metadata.disc = parseFraction(value);
      else if (key == PARENT_METADATA_FIELD_COPYRIGHT)
        metadata.copyright = value;
      else if (key == PARENT_METADATA_FIELD_GENRE)
        metadata.genre = value;
      else if (key == PARENT_METADATA_FIELD_COMMENT)
        metadata.comment = value;
      else if (key == PARENT_METADATA_FIELD_ALBUM_ARTIST)
        metadata.album_artist = value;
      else if (key == PARENT_METADATA_FIELD_TSRC)
        metadata.tsrc = value;
      else if (key == PARENT_METADATA_FIELD_ENCODER)
        metadata.encoder = value;
      else if (key == PARENT_METADATA_FIELD_ENCODED_BY)
        metadata.encoded_by = value;
      else if (key == PARENT_METADATA_FIELD_DATE)
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
    tomlGen.addTableValue(parent, PARENT_STREAM_FIELD_CODEC, stream.codec);
    tomlGen.addTableValue(parent, PARENT_STREAM_FIELD_TYPE, stream.type);
    tomlGen.addTableValue(parent, PARENT_STREAM_FIELD_SAMPLE_RATE, stream.sample_rate);
    tomlGen.addTableValue(parent, PARENT_STREAM_FIELD_CHANNELS, stream.channels);
    tomlGen.addTableValue(parent, PARENT_STREAM_FIELD_BITRATE, stream.bitrate);
    tomlGen.addTableValue(parent, PARENT_STREAM_FIELD_SAMPLE_FORMAT, stream.sample_format);
    tomlGen.addTableValue(parent, PARENT_STREAM_FIELD_CHANNEL_LAYOUT, stream.channel_layout);
  }
};
