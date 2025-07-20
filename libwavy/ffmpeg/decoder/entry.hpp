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

#include <libwavy/common/api/entry.hpp>
#include <libwavy/common/macros.hpp>
#include <libwavy/common/state.hpp>
#include <libwavy/common/types.hpp>
#include <libwavy/utils/io/dbg/entry.hpp>
#include <libwavy/utils/math/entry.hpp>
#include <vector>

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/audio_fifo.h>
#include <libavutil/mem.h>
#include <libavutil/opt.h>
}

inline constexpr const ByteCount DefaultAVIOBufferSize = 32768;

// Custom AVIO read function
static auto readAVIO(void* opaque, AudioByte* buf, int buf_size) -> int
{
  auto*              segments      = static_cast<TotalAudioData*>(opaque);
  static size_t      segment_index = 0;
  static AudioOffset read_offset   = 0;

  if (segment_index >= segments->size())
  {
    return AVERROR_EOF; // No more data
  }

  size_t segment_size  = (*segments)[segment_index].size();
  size_t bytes_to_copy = std::min(static_cast<size_t>(buf_size), segment_size - read_offset);

  memcpy(buf, (*segments)[segment_index].data() + read_offset, bytes_to_copy);
  read_offset += bytes_to_copy;

  if (read_offset >= segment_size)
  {
    // Move to the next segment
    segment_index++;
    read_offset = 0;
  }

  return bytes_to_copy;
}

namespace libwavy::ffmpeg
{

/**
 * @class MediaDecoder
 * @brief Decodes transport stream audio for playback from a vector
 */
class WAVY_API MediaDecoder
{
public:
  MediaDecoder();
  ~MediaDecoder();

  auto decode(TotalAudioData& ts_segments, TotalDecodedAudioData& output_audio) -> bool;

private:
  auto is_lossless_codec(AVCodecID codec_id) -> bool;

  void print_audio_metadata(AVFormatContext* formatCtx, AVCodecParameters* codecParams);
  auto init_input_context(TotalAudioData& ts_segments, AVFormatContext*& ctx) -> bool;
  void detect_format(AVFormatContext* ctx);
  auto find_audio_stream(AVFormatContext* ctx, AudioStreamIdx& index) -> bool;
  auto setup_codec(AVCodecParameters* codec_params, AVCodecContext*& codec_ctx) -> bool;
  auto process_packets(AVFormatContext* ctx, AVCodecContext* codec_ctx, AudioStreamIdx stream_idx,
                       AVCodecParameters* codec_params, TotalDecodedAudioData& output) -> bool;
  void cleanup(AVFormatContext* ctx, AVCodecContext* codec_ctx = nullptr);
};

} // namespace libwavy::ffmpeg
