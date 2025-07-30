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

#include <cstdlib>
#include <libwavy/common/state.hpp>
#include <string_view>
#include <toml++/toml.hpp>

namespace TomlKeys
{
namespace Owner
{
inline constexpr auto OwnerID  = "owner_id";
inline constexpr auto Nickname = "nickname";
} // namespace Owner
namespace Audio
{
inline constexpr auto Parser          = "audio_parser";
inline constexpr auto Bitrate         = "bitrate";
inline constexpr auto Duration        = "duration";
inline constexpr auto Path            = "path";
inline constexpr auto FileFormat      = "file_format";
inline constexpr auto FileFormatLong  = "file_format_long";
inline constexpr auto TranscodedRates = "transcoded_bitrates";
} // namespace Audio

namespace Metadata
{
inline constexpr auto Root        = "metadata";
inline constexpr auto TSRC        = "TSRC";
inline constexpr auto Album       = "album";
inline constexpr auto AlbumArtist = "album_artist";
inline constexpr auto Artist      = "artist";
inline constexpr auto Comment     = "comment";
inline constexpr auto Copyright   = "copyright";
inline constexpr auto Date        = "date";
inline constexpr auto Disc        = "disc";
inline constexpr auto EncodedBy   = "encoded_by";
inline constexpr auto Encoder     = "encoder";
inline constexpr auto Genre       = "genre";
inline constexpr auto Title       = "title";
inline constexpr auto Track       = "track";
} // namespace Metadata

namespace Stream
{
inline constexpr auto Stream0       = "stream_0";
inline constexpr auto Stream1       = "stream_1";
inline constexpr auto Bitrate       = "bitrate";
inline constexpr auto ChannelLayout = "channel_layout";
inline constexpr auto Channels      = "channels";
inline constexpr auto Codec         = "codec";
inline constexpr auto SampleFormat  = "sample_format";
inline constexpr auto SampleRate    = "sample_rate";
inline constexpr auto Type          = "type";
} // namespace Stream
} // namespace TomlKeys

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
  result.bitrate     = metadata[TomlKeys::Audio::Parser][TomlKeys::Audio::Bitrate].value_or(-1);
  result.duration    = metadata[TomlKeys::Audio::Parser][TomlKeys::Audio::Duration].value_or(-1);
  result.path        = metadata[TomlKeys::Audio::Parser][TomlKeys::Audio::Path].value_or(""s);
  result.file_format = metadata[TomlKeys::Audio::Parser][TomlKeys::Audio::FileFormat].value_or(""s);
  result.file_format_long =
    metadata[TomlKeys::Audio::Parser][TomlKeys::Audio::FileFormatLong].value_or(""s);

  if (auto bitrates_array =
        metadata[TomlKeys::Audio::Parser][TomlKeys::Audio::TranscodedRates].as_array())
  {
    for (const auto& val : *bitrates_array)
    {
      if (val.is_integer())
        result.bitrates.push_back(static_cast<int>(val.as_integer()->get()));
    }
  }

  // Metadata Section
  result.tsrc  = metadata[TomlKeys::Metadata::Root][TomlKeys::Metadata::TSRC].value_or(""s);
  result.album = metadata[TomlKeys::Metadata::Root][TomlKeys::Metadata::Album].value_or(""s);
  result.album_artist =
    metadata[TomlKeys::Metadata::Root][TomlKeys::Metadata::AlbumArtist].value_or(""s);
  result.artist  = metadata[TomlKeys::Metadata::Root][TomlKeys::Metadata::Artist].value_or(""s);
  result.comment = metadata[TomlKeys::Metadata::Root][TomlKeys::Metadata::Comment].value_or(""s);
  result.copyright =
    metadata[TomlKeys::Metadata::Root][TomlKeys::Metadata::Copyright].value_or(""s);
  result.date = metadata[TomlKeys::Metadata::Root][TomlKeys::Metadata::Date].value_or(""s);
  result.encoded_by =
    metadata[TomlKeys::Metadata::Root][TomlKeys::Metadata::EncodedBy].value_or(""s);
  result.encoder = metadata[TomlKeys::Metadata::Root][TomlKeys::Metadata::Encoder].value_or(""s);
  result.genre   = metadata[TomlKeys::Metadata::Root][TomlKeys::Metadata::Genre].value_or(""s);
  result.title   = metadata[TomlKeys::Metadata::Root][TomlKeys::Metadata::Title].value_or(""s);

  result.track =
    parseFraction(metadata[TomlKeys::Metadata::Root][TomlKeys::Metadata::Track].value_or(""s));
  result.disc =
    parseFraction(metadata[TomlKeys::Metadata::Root][TomlKeys::Metadata::Disc].value_or(""s));

  // Audio Stream
  if (metadata.contains(TomlKeys::Stream::Stream0))
  {
    const auto& audio_stream = metadata[TomlKeys::Stream::Stream0].as_table();
    if (audio_stream)
    {
      result.audio_stream.codec       = audio_stream->at(TomlKeys::Stream::Codec).value_or(""s);
      result.audio_stream.type        = audio_stream->at(TomlKeys::Stream::Type).value_or(""s);
      result.audio_stream.bitrate     = audio_stream->at(TomlKeys::Stream::Bitrate).value_or(-1);
      result.audio_stream.sample_rate = audio_stream->at(TomlKeys::Stream::SampleRate).value_or(-1);
      result.audio_stream.channels    = audio_stream->at(TomlKeys::Stream::Channels).value_or(-1);
      result.audio_stream.channel_layout =
        audio_stream->at(TomlKeys::Stream::ChannelLayout).value_or(""s);
      result.audio_stream.sample_format =
        audio_stream->at(TomlKeys::Stream::SampleFormat).value_or(""s);
    }
  }

  // Video Stream
  if (metadata.contains(TomlKeys::Stream::Stream1))
  {
    const auto& video_stream = metadata[TomlKeys::Stream::Stream1].as_table();
    if (video_stream)
    {
      result.video_stream.codec       = video_stream->at(TomlKeys::Stream::Codec).value_or(""s);
      result.video_stream.type        = video_stream->at(TomlKeys::Stream::Type).value_or(""s);
      result.video_stream.bitrate     = video_stream->at(TomlKeys::Stream::Bitrate).value_or(-1);
      result.video_stream.sample_rate = video_stream->at(TomlKeys::Stream::SampleRate).value_or(-1);
      result.video_stream.channels    = video_stream->at(TomlKeys::Stream::Channels).value_or(-1);
      result.video_stream.channel_layout =
        video_stream->at(TomlKeys::Stream::ChannelLayout).value_or(""s);
      result.video_stream.sample_format =
        video_stream->at(TomlKeys::Stream::SampleFormat).value_or(""s);
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
