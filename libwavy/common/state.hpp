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

#include <iterator>
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
  TotalAudioData            transport_segments;
  mutable std::shared_mutex mutex_; // allows multiple readers, exclusive writer

public:
  void appendSegment(AudioData&& segment)
  {
    std::unique_lock lock(mutex_);
    transport_segments.emplace_back(std::move(segment));
  }

  // Append initial segment + multiple segments safely
  void appendSegmentsFLAC(AudioData&& initSegment, TotalAudioData&& m4sSegments)
  {
    std::unique_lock lock(mutex_); // exclusive lock for write
    transport_segments.push_back(std::move(initSegment));
    transport_segments.insert(transport_segments.end(),
                              std::make_move_iterator(m4sSegments.begin()),
                              std::make_move_iterator(m4sSegments.end()));
  }

  // Get a snapshot copy of all segments safely
  auto getAllSegments() const -> TotalAudioData
  {
    std::shared_lock lock(mutex_);
    return transport_segments;
  }

  auto getSegment(size_t index) const -> AudioData
  {
    std::shared_lock lock(mutex_);
    if (index >= transport_segments.size())
      return {}; // or throw or handle error
    return transport_segments[index];
  }

  // check whether transport_segments is empty safely
  auto segsEmpty() const -> bool
  {
    std::shared_lock lock(mutex_);
    return transport_segments.empty();
  }

  // Clear all segments safely
  void clearSegments()
  {
    std::unique_lock lock(mutex_);
    transport_segments.clear();
  }

  // Get count safely
  auto segSizeAll() const -> size_t
  {
    std::shared_lock lock(mutex_);
    return transport_segments.size();
  }
};
