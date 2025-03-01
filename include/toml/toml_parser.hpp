#pragma once

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
  int    bitrate;
  int    duration;
  string path;
  string file_format;
  string file_format_long;

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
