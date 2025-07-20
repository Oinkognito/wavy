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

#include <libwavy/common/types.hpp>
#include <libwavy/logger.hpp>

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avassert.h>
#include <libavutil/mathematics.h>
#include <libavutil/opt.h>
#include <libavutil/samplefmt.h>
#include <libswresample/swresample.h>
}

#include <cmath>

// $NOTE
//
// When we detect inf/NaN in frames, currently I am trying to just silence them but in reality
// the audio has a slightly high pitch uptick (glitch like noise) for a brief moment of that sample
//
// This is called having a transcoding artifact and this issue is for mp3 -> mp3 transcoding ONLY
//
// Not sure on how to fix it at this moment.
//
// But this is great step up, this transcoder seems to be working with EVERY mp3 file I have tested it with.
//
// -> Also minimum bitrate for transcoding is 32kbps by default (i think it is set by LAME itself)
//
// This Transcoder should work for the following:
//
// flac -> mp3 transcoding (16, 32 bit tested and verified)
// mp3 -> mp3 transcoding

// SCALE_FACTOR_32B: Scaling factor for 32 bit audio stream (float by default)
inline constexpr float SCALE_FACTOR_32B = 1.0f / static_cast<float>(1 << 31);
// SCALE_FACTOR_16B: Scaling factor for 16 bit audio stream (float by default)
inline constexpr float SCALE_FACTOR_16B = 1.0f / static_cast<float>(1 << 15);
// FLOAT_TO_INT32: Converstion metric needed to convert float scaling factor to int32
inline constexpr float FLOAT_TO_INT32 = static_cast<float>(1 << 31);
// FLOAT_TO_INT16: Converstion metric needed to convert float scaling factor to int16
inline constexpr float FLOAT_TO_INT16 = static_cast<float>(1 << 15);

namespace libwavy::ffmpeg
{

class Transcoder
{
private:
  AVFormatContext* in_format_ctx   = nullptr; // Format context for the input media file
  AVFormatContext* out_format_ctx  = nullptr; // Format context for the output media file
  AVCodecContext*  in_codec_ctx    = nullptr; // Codec context for decoding input audio
  AVCodecContext*  out_codec_ctx   = nullptr; // Codec context for encoding output audio
  SwrContext*      swr_ctx         = nullptr; // Resampler context for audio format conversion
  AVStream*        out_stream      = nullptr; // Output stream to write encoded audio
  AVPacket*        packet          = av_packet_alloc(); // Container for encoded/compressed data
  AVFrame*         frame           = av_frame_alloc();  // Raw decoded audio frame
  AVFrame*         resampled_frame = av_frame_alloc();  // Frame holding resampled audio data

public:
  void print_audio_info(CStrRelPath filename, AVFormatContext* format_ctx,
                        AVCodecContext* codec_ctx, const char* label);

  ///* Removing the transcoding artifacts through the current implementation seems to be working WITHOUT destroying existing audio data.
  ///  if this is causing any audio data loss or unneeded manipulation, feel free to open an issue and explain in detail. *///
  template <typename T, typename F, typename R>
  void process_samples(AVFrame* frame, F convert_to_float, R convert_from_float);

  inline auto soft_clip(float x) -> float
  {
    return std::tanh(x); // Smoothly compresses extreme values
  }

  inline auto soft_clip(double x) -> double { return std::tanh(x); }

  // Function to sanitize audio samples (to avoid any energy issues from psymodel or quantize source files of libmp3lame)
  void sanitize_audio_samples(AVFrame* frame);

  // Main transcoding function - now more modular
  auto transcode_to_mp3(CStrRelPath input_filename, CStrRelPath output_filename,
                        const int given_bitrate) -> int;

  // Function to initialize the input file and decoder
  auto initialize_input(CStrRelPath input_filename, AVFormatContext** in_format_ctx,
                        AVCodecContext** in_codec_ctx, AudioStreamIdx* audio_stream_index) -> int;

  // Function to initialize the output file and encoder
  auto initialize_output(CStrRelPath output_filename, AVFormatContext** out_format_ctx,
                         AVCodecContext** out_codec_ctx, AVStream** out_stream,
                         AVCodecContext* in_codec_ctx, int bitrate) -> int;

  // Function to initialize the audio resampler
  auto initialize_resampler(SwrContext** swr_ctx, AVCodecContext* in_codec_ctx,
                            AVCodecContext* out_codec_ctx) -> int;

  // Function to prepare and allocate frames and packets
  auto prepare_frames(AVFrame** frame, AVFrame** resampled_frame, AVPacket** packet,
                      AVCodecContext* out_codec_ctx) -> int;

  // Function to process and transcode audio frames
  auto process_audio_frames(AVFormatContext* in_format_ctx, AVCodecContext* in_codec_ctx,
                            AVCodecContext* out_codec_ctx, AVFormatContext* out_format_ctx,
                            SwrContext* swr_ctx, AVFrame* frame, AVFrame* resampled_frame,
                            AVPacket* packet, AudioStreamIdx audio_stream_index) -> int;

  // Function to decode an audio packet and process frames
  auto decode_audio_packet(AVCodecContext* in_codec_ctx, AVPacket* packet, AVFrame* frame,
                           AVCodecContext* out_codec_ctx, AVFormatContext* out_format_ctx,
                           SwrContext* swr_ctx, AVFrame* resampled_frame, int64_t* next_pts,
                           int* samples_sanitized) -> int;

  // Function to resample and encode a single audio frame
  auto resample_and_encode_frame(AVFrame* frame, AVFrame* resampled_frame, SwrContext* swr_ctx,
                                 AVCodecContext* out_codec_ctx, AVFormatContext* out_format_ctx,
                                 int64_t* next_pts, int* samples_sanitized) -> int;

  // Function to encode and write a single audio frame
  auto encode_audio_frame(AVFrame* frame, AVCodecContext* codec_ctx, AVFormatContext* format_ctx)
    -> int;

  auto flush_resampler(SwrContext* swr_ctx, AVFrame* resampled_frame, AVCodecContext* out_codec_ctx,
                       AVFormatContext* out_format_ctx, int64_t* next_pts, int* samples_sanitized)
    -> int;

  // Function to flush encoder and write remaining packets
  auto flush_encoder(AVCodecContext* codec_ctx, AVFormatContext* format_ctx) -> int;

  // Function to clean up and free all allocated resources
  void cleanup_resources(AVFormatContext** in_format_ctx, AVFormatContext** out_format_ctx,
                         AVCodecContext** in_codec_ctx, AVCodecContext** out_codec_ctx,
                         SwrContext** swr_ctx, AVFrame** frame, AVFrame** resampled_frame,
                         AVPacket** packet);
};

} // namespace libwavy::ffmpeg
