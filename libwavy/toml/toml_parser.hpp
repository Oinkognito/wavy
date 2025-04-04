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

#include "toml.hpp"
#include <cstdlib>
#include <filesystem>
#include <string_view>

using namespace std;
namespace toml_fs = std::filesystem;

#define PARENT_AUDIO_PARSER                 "audio_parser"
#define PARENT_AUDIO_FIELD_BITRATE          "bitrate"
#define PARENT_AUDIO_FIELD_DURATION         "duration"
#define PARENT_AUDIO_FIELD_PATH             "path"
#define PARENT_AUDIO_FIELD_FILE_FORMAT      "file_format"
#define PARENT_AUDIO_FIELD_FILE_FORMAT_LONG "file_format_long"
#define PARENT_AUDIO_FIELD_TRNS_BITRATES    "transcoded_bitrates"

#define PARENT_METADATA                    "metadata"
#define PARENT_METADATA_FIELD_TSRC         "TSRC"
#define PARENT_METADATA_FIELD_ALBUM        "album"
#define PARENT_METADATA_FIELD_ALBUM_ARTIST "album_artist"
#define PARENT_METADATA_FIELD_ARTIST       "artist"
#define PARENT_METADATA_FIELD_COMMENT      "comment"
#define PARENT_METADATA_FIELD_COPYRIGHT    "copyright"
#define PARENT_METADATA_FIELD_DATE         "date"
#define PARENT_METADATA_FIELD_DISC         "disc"
#define PARENT_METADATA_FIELD_ENCODED_BY   "encoded_by"
#define PARENT_METADATA_FIELD_ENCODER      "encoder"
#define PARENT_METADATA_FIELD_GENRE        "genre"
#define PARENT_METADATA_FIELD_TITLE        "title"
#define PARENT_METADATA_FIELD_TRACK        "track"

#define PARENT_STREAM_0                    "stream_0"
#define PARENT_STREAM_1                    "stream_1"
#define PARENT_STREAM_FIELD_BITRATE        "bitrate"
#define PARENT_STREAM_FIELD_CHANNEL_LAYOUT "channel_layout"
#define PARENT_STREAM_FIELD_CHANNELS       "channels"
#define PARENT_STREAM_FIELD_CODEC          "codec"
#define PARENT_STREAM_FIELD_SAMPLE_FORMAT  "sample_format"
#define PARENT_STREAM_FIELD_SAMPLE_RATE    "sample_rate"
#define PARENT_STREAM_FIELD_TYPE           "type"

struct StreamMetadata
{
  string codec;
  string type;
  int    bitrate;
  int    sample_rate;
  int    channels;
  string channel_layout;
  string sample_format;
};

struct AudioMetadata
{
  int         bitrate;
  int         duration;
  string      path;
  string      file_format;
  string      file_format_long;
  vector<int> bitrates;

  string         title;
  string         artist;
  string         album;
  pair<int, int> track;
  pair<int, int> disc;
  string         copyright;
  string         genre;
  string         comment;
  string         album_artist;
  string         tsrc;
  string         encoder;
  string         encoded_by;
  string         date;

  StreamMetadata audio_stream;
  StreamMetadata video_stream;
};

// Parses a fraction (e.g., "6/12")
inline auto parseFraction(string_view value) -> pair<int, int>
{
  size_t pos = value.find('/');
  if (pos == string::npos)
    return {stoi(string(value)), 0};
  return {stoi(string(value.substr(0, pos))), stoi(string(value.substr(pos + 1)))};
}

inline auto parseAudioMetadataFromTomlTable(const toml::table& metadata) -> AudioMetadata
{
  AudioMetadata result;

  // Audio Parser Section
  result.bitrate     = metadata[PARENT_AUDIO_PARSER][PARENT_AUDIO_FIELD_BITRATE].value_or(-1);
  result.duration    = metadata[PARENT_AUDIO_PARSER][PARENT_AUDIO_FIELD_DURATION].value_or(-1);
  result.path        = metadata[PARENT_AUDIO_PARSER][PARENT_AUDIO_FIELD_PATH].value_or(""s);
  result.file_format = metadata[PARENT_AUDIO_PARSER][PARENT_AUDIO_FIELD_FILE_FORMAT].value_or(""s);
  result.file_format_long =
    metadata[PARENT_AUDIO_PARSER][PARENT_AUDIO_FIELD_FILE_FORMAT_LONG].value_or(""s);
  if (auto bitrates_array =
        metadata[PARENT_AUDIO_PARSER][PARENT_AUDIO_FIELD_TRNS_BITRATES].as_array())
  {
    for (const auto& val : *bitrates_array)
    {
      if (val.is_integer())
      {
        result.bitrates.push_back(static_cast<int>(val.as_integer()->get()));
      }
    }
  }

  // Metadata Section
  result.tsrc         = metadata[PARENT_METADATA][PARENT_METADATA_FIELD_TSRC].value_or(""s);
  result.album        = metadata[PARENT_METADATA][PARENT_METADATA_FIELD_ALBUM].value_or(""s);
  result.album_artist = metadata[PARENT_METADATA][PARENT_METADATA_FIELD_ALBUM_ARTIST].value_or(""s);
  result.artist       = metadata[PARENT_METADATA][PARENT_METADATA_FIELD_ARTIST].value_or(""s);
  result.comment      = metadata[PARENT_METADATA][PARENT_METADATA_FIELD_COMMENT].value_or(""s);
  result.copyright    = metadata[PARENT_METADATA][PARENT_METADATA_FIELD_COPYRIGHT].value_or(""s);
  result.date         = metadata[PARENT_METADATA][PARENT_METADATA_FIELD_DATE].value_or(""s);
  result.encoded_by   = metadata[PARENT_METADATA][PARENT_METADATA_FIELD_ENCODED_BY].value_or(""s);
  result.encoder      = metadata[PARENT_METADATA][PARENT_METADATA_FIELD_ENCODER].value_or(""s);
  result.genre        = metadata[PARENT_METADATA][PARENT_METADATA_FIELD_GENRE].value_or(""s);
  result.title        = metadata[PARENT_METADATA][PARENT_METADATA_FIELD_TITLE].value_or(""s);

  result.track =
    parseFraction(metadata[PARENT_METADATA][PARENT_METADATA_FIELD_TRACK].value_or(""s));
  result.disc = parseFraction(metadata[PARENT_METADATA][PARENT_METADATA_FIELD_DISC].value_or(""s));

  // Audio Stream Metadata
  if (metadata.contains(PARENT_STREAM_0))
  {
    const auto& audio_stream = metadata[PARENT_STREAM_0].as_table();
    if (audio_stream)
    {
      result.audio_stream.codec   = audio_stream->at(PARENT_STREAM_FIELD_CODEC).value_or(""s);
      result.audio_stream.type    = audio_stream->at(PARENT_STREAM_FIELD_TYPE).value_or(""s);
      result.audio_stream.bitrate = audio_stream->at(PARENT_STREAM_FIELD_BITRATE).value_or(-1);
      result.audio_stream.sample_rate =
        audio_stream->at(PARENT_STREAM_FIELD_SAMPLE_RATE).value_or(-1);
      result.audio_stream.channels = audio_stream->at(PARENT_STREAM_FIELD_CHANNELS).value_or(-1);
      result.audio_stream.channel_layout =
        audio_stream->at(PARENT_STREAM_FIELD_CHANNEL_LAYOUT).value_or(""s);
      result.audio_stream.sample_format =
        audio_stream->at(PARENT_STREAM_FIELD_SAMPLE_FORMAT).value_or(""s);
    }
  }

  // Video Stream Metadata
  if (metadata.contains(PARENT_STREAM_1))
  {
    const auto& video_stream = metadata[PARENT_STREAM_1].as_table();
    if (video_stream)
    {
      result.video_stream.codec   = video_stream->at(PARENT_STREAM_FIELD_CODEC).value_or(""s);
      result.video_stream.type    = video_stream->at(PARENT_STREAM_FIELD_TYPE).value_or(""s);
      result.video_stream.bitrate = video_stream->at(PARENT_STREAM_FIELD_BITRATE).value_or(-1);
      result.video_stream.sample_rate =
        video_stream->at(PARENT_STREAM_FIELD_SAMPLE_RATE).value_or(-1);
      result.video_stream.channels = video_stream->at(PARENT_STREAM_FIELD_CHANNELS).value_or(-1);
      result.video_stream.channel_layout =
        video_stream->at(PARENT_STREAM_FIELD_CHANNEL_LAYOUT).value_or(""s);
      result.video_stream.sample_format =
        video_stream->at(PARENT_STREAM_FIELD_SAMPLE_FORMAT).value_or(""s);
    }
  }

  return result;
}

inline auto parseAudioMetadata(const string& filePath) -> AudioMetadata
{
  auto metadata = toml::parse_file(filePath);
  return parseAudioMetadataFromTomlTable(metadata);
}

// Parse Metadata Directly from a TOML String
inline auto parseAudioMetadataFromDataString(const string& dataString) -> AudioMetadata
{
  auto metadata = toml::parse(dataString);
  return parseAudioMetadataFromTomlTable(metadata);
}
