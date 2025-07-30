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
