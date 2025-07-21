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
  TotalAudioData            m_transportSegments;
  mutable std::shared_mutex m_mutex; // allows multiple readers, exclusive writer

public:
  void appendSegment(AudioData&& segment)
  {
    std::unique_lock lock(m_mutex);
    m_transportSegments.emplace_back(std::move(segment));
  }

  // Append initial segment + multiple segments safely
  void appendSegmentsFLAC(AudioData&& initSegment, TotalAudioData&& m4sSegments)
  {
    std::unique_lock lock(m_mutex); // exclusive lock for write
    m_transportSegments.push_back(std::move(initSegment));
    m_transportSegments.insert(m_transportSegments.end(),
                               std::make_move_iterator(m4sSegments.begin()),
                               std::make_move_iterator(m4sSegments.end()));
  }

  // Get a snapshot copy of all segments safely
  auto getAllSegments() const -> TotalAudioData
  {
    std::shared_lock lock(m_mutex);
    return m_transportSegments;
  }

  auto getSegment(size_t index) const -> AudioData
  {
    std::shared_lock lock(m_mutex);
    if (index >= m_transportSegments.size())
      return {}; // or throw or handle error
    return m_transportSegments[index];
  }

  // check whether m_transportSegments is empty safely
  auto segsEmpty() const -> bool
  {
    std::shared_lock lock(m_mutex);
    return m_transportSegments.empty();
  }

  // Clear all segments safely
  void clearSegments()
  {
    std::unique_lock lock(m_mutex);
    m_transportSegments.clear();
  }

  // Get count safely
  auto segSizeAll() const -> size_t
  {
    std::shared_lock lock(m_mutex);
    return m_transportSegments.size();
  }
};
