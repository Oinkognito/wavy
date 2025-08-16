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

#include <iterator>
#include <libwavy/common/types.hpp>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <string>
#include <vector>

using AudioData             = std::string;
using TotalAudioData        = std::vector<AudioData>;
using DecodedAudioData      = unsigned char;
using TotalDecodedAudioData = std::vector<DecodedAudioData>;

struct as
{
  static auto str(const std::vector<char>& v) -> std::string { return {v.begin(), v.end()}; }
  static auto str(std::vector<ui8>& v) -> std::string { return {v.begin(), v.end()}; }
  static auto vchar(const std::string& k) -> std::vector<char> { return {k.begin(), k.end()}; }
};

inline constexpr const size_t MAX_STR_LEN  = 128;
inline constexpr const size_t MAX_PATH_LEN = 256;
inline constexpr const size_t MAX_BITRATES = 16;

using namespace std;

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

struct StreamMetadataPlain
{
  chararr<MAX_STR_LEN> codec{};
  chararr<MAX_STR_LEN> type{};
  int                  bitrate{};
  int                  sample_rate{};
  int                  channels{};
  chararr<MAX_STR_LEN> channel_layout{};
  chararr<MAX_STR_LEN> sample_format{};
};

struct AudioMetadataPlain
{
  chararr<MAX_STR_LEN> nickname{};

  int                   bitrate{};
  int                   duration{};
  chararr<MAX_PATH_LEN> path{};
  chararr<MAX_STR_LEN>  file_format{};
  chararr<MAX_STR_LEN>  file_format_long{};
  chararr<MAX_BITRATES> bitrates{};
  int                   bitrates_count{};

  chararr<MAX_STR_LEN> title{};
  chararr<MAX_STR_LEN> artist{};
  chararr<MAX_STR_LEN> album{};
  int                  track_first{};
  int                  track_second{};
  int                  disc_first{};
  int                  disc_second{};
  chararr<MAX_STR_LEN> copyright{};
  chararr<MAX_STR_LEN> genre{};
  chararr<MAX_STR_LEN> comment{};
  chararr<MAX_STR_LEN> album_artist{};
  chararr<MAX_STR_LEN> tsrc{};
  chararr<MAX_STR_LEN> encoder{};
  chararr<MAX_STR_LEN> encoded_by{};
  chararr<MAX_STR_LEN> date{};

  StreamMetadataPlain audio_stream{};
  StreamMetadataPlain video_stream{};
};

struct AudioMetadata
{
  StorageOwnerID nickname;

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

  [[nodiscard]] auto to_plain() const -> AudioMetadataPlain
  {
    AudioMetadataPlain out{};

    auto copy_str = [](chararr<MAX_STR_LEN>& dest, const std::string& src)
    { std::snprintf(dest.data(), dest.size(), "%s", src.c_str()); };
    auto copy_path = [](chararr<MAX_PATH_LEN>& dest, const std::string& src)
    { std::snprintf(dest.data(), dest.size(), "%s", src.c_str()); };

    copy_str(out.nickname, nickname);
    out.bitrate  = bitrate;
    out.duration = duration;
    copy_path(out.path, path);
    copy_str(out.file_format, file_format);
    copy_str(out.file_format_long, file_format_long);

    out.bitrates_count = std::min<int>(bitrates.size(), MAX_BITRATES);
    for (int i = 0; i < out.bitrates_count; i++)
    {
      out.bitrates[i] = bitrates[i];
    }

    copy_str(out.title, title);
    copy_str(out.artist, artist);
    copy_str(out.album, album);

    out.track_first  = track.first;
    out.track_second = track.second;
    out.disc_first   = disc.first;
    out.disc_second  = disc.second;

    copy_str(out.copyright, copyright);
    copy_str(out.genre, genre);
    copy_str(out.comment, comment);
    copy_str(out.album_artist, album_artist);
    copy_str(out.tsrc, tsrc);
    copy_str(out.encoder, encoder);
    copy_str(out.encoded_by, encoded_by);
    copy_str(out.date, date);

    auto copy_stream = [&](const StreamMetadata& src, StreamMetadataPlain& dst)
    {
      copy_str(dst.codec, src.codec);
      copy_str(dst.type, src.type);
      dst.bitrate     = src.bitrate;
      dst.sample_rate = src.sample_rate;
      dst.channels    = src.channels;
      copy_str(dst.channel_layout, src.channel_layout);
      copy_str(dst.sample_format, src.sample_format);
    };

    copy_stream(audio_stream, out.audio_stream);
    copy_stream(video_stream, out.video_stream);

    return out;
  }
};

// This is supposed to be thread safe concurrent storage solution for
// AudioData and DecodedAudioData
struct GlobalState
{
private:
  TotalAudioData            m_segments;
  std::optional<AudioData>  m_initSegmentFLAC; // stores init segment if needed
  mutable std::shared_mutex m_mutex;

public:
  // Mode 1: Normal constructor (no init segment)
  GlobalState() = default;

  // Mode 2: FLAC init segment passed at construction
  explicit GlobalState(AudioData initSegment) : m_initSegmentFLAC(std::move(initSegment))
  {
    m_segments.push_back(*m_initSegmentFLAC); // auto-store it
  }

  /// Append a segment (used for streaming mode)
  void appendSegment(AudioData&& segment)
  {
    std::unique_lock lock(m_mutex);
    m_segments.emplace_back(std::move(segment));
  }

  /// Append multiple segments (used for prefetching)
  void appendSegments(TotalAudioData&& segments)
  {
    std::unique_lock lock(m_mutex);
    m_segments.insert(m_segments.end(), std::make_move_iterator(segments.begin()),
                      std::make_move_iterator(segments.end()));
  }

  /// Get a snapshot of all segments
  auto getAllSegments() const -> TotalAudioData
  {
    std::shared_lock lock(m_mutex);
    return m_segments;
  }

  /// Get a single segment by index
  auto getSegment(size_t index) const -> AudioData
  {
    std::shared_lock lock(m_mutex);
    if (index >= m_segments.size())
      return {}; // when calling this, do a sanity .empty check
    return m_segments[index];
  }

  /// Whether the store is empty
  auto segsEmpty() const -> bool
  {
    std::shared_lock lock(m_mutex);
    return m_segments.empty();
  }

  /// Clear all segments (used in teardown/reset)
  void clearSegments()
  {
    std::unique_lock lock(m_mutex);
    m_segments.clear();
    if (m_initSegmentFLAC)
      m_segments.push_back(*m_initSegmentFLAC); // re-inject init if needed
  }

  /// Get number of stored segments
  auto segSizeAll() const -> size_t
  {
    std::shared_lock lock(m_mutex);
    return m_segments.size();
  }

  /// Access to init segment for diagnostic/logging (if needed)
  auto hasInitSegmentFLAC() const -> bool { return m_initSegmentFLAC.has_value(); }
};
