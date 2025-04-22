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

#include <algorithm>
#include <cmath>
#include <string>

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
constexpr float SCALE_FACTOR_32B = 1.0f / static_cast<float>(1 << 31);
// SCALE_FACTOR_16B: Scaling factor for 16 bit audio stream (float by default)
constexpr float SCALE_FACTOR_16B = 1.0f / static_cast<float>(1 << 15);
// FLOAT_TO_INT32: Converstion metric needed to convert float scaling factor to int32
constexpr float FLOAT_TO_INT32 = static_cast<float>(1 << 31);
// FLOAT_TO_INT16: Converstion metric needed to convert float scaling factor to int16
constexpr float FLOAT_TO_INT16 = static_cast<float>(1 << 15);

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
  void print_audio_info(const char* filename, AVFormatContext* format_ctx,
                        AVCodecContext* codec_ctx, const char* label)
  {
    LOG_DEBUG << TRANSCODER_LOG << "========== " << label << " ===============";
    LOG_DEBUG << TRANSCODER_LOG << "File: " << filename;
    LOG_DEBUG << TRANSCODER_LOG << "Codec: " << codec_ctx->codec->long_name << " ("
              << codec_ctx->codec->name << ")";
    LOG_DEBUG << TRANSCODER_LOG << "Bitrate: " << codec_ctx->bit_rate / 1000 << " kbps";
    LOG_DEBUG << TRANSCODER_LOG << "Sample Rate: " << codec_ctx->sample_rate << " Hz";
    LOG_DEBUG << TRANSCODER_LOG << "Channels: " << codec_ctx->ch_layout.nb_channels;
    LOG_DEBUG << TRANSCODER_LOG
              << "Sample Format: " << av_get_sample_fmt_name(codec_ctx->sample_fmt);
    LOG_DEBUG << TRANSCODER_LOG << "Duration: " << format_ctx->duration / AV_TIME_BASE << " sec";
    char layout_desc[256];
    av_channel_layout_describe(&codec_ctx->ch_layout, layout_desc, sizeof(layout_desc));
    LOG_DEBUG << TRANSCODER_LOG << "Channel Layout Description: " << layout_desc;
    LOG_DEBUG << TRANSCODER_LOG << "=================================================";
  }

  ///* Removing the transcoding artifacts through the current implementation seems to be working WITHOUT destroying existing audio data.
  ///  if this is causing any audio data loss or unneeded manipulation, feel free to open an issue and explain in detail. *///
  template <typename T, typename F, typename R>
  void process_samples(AVFrame* frame, F convert_to_float, R convert_from_float)
  {
    if (!frame)
      return;
    int  format           = frame->format;
    bool is_planar        = av_sample_fmt_is_planar(static_cast<AVSampleFormat>(format));
    int  channels         = frame->ch_layout.nb_channels;
    int  samples          = frame->nb_samples;
    bool found_invalid    = false;
    bool found_high_pitch = false;

    // Parameters for high pitch detection
    const float threshold        = 0.85f; // Threshold for high amplitude
    const int   window_size      = 5;     // Window size for checking rapid changes
    const float change_threshold = 0.5f;  // Threshold for rapid amplitude changes

    auto clamp = [](auto x)
    {
      using U = decltype(x);
      return std::max(static_cast<U>(-1.0), std::min(static_cast<U>(1.0), x));
    };

    // Store previous samples for high pitch detection
    std::vector<std::vector<float>> prev_samples(channels, std::vector<float>(window_size, 0.0f));

    for (int ch = 0; ch < channels; ++ch)
    {
      auto* data   = reinterpret_cast<T*>(is_planar ? frame->extended_data[ch] : frame->data[0]);
      int   stride = is_planar ? 1 : channels;

      // First pass: Convert all samples to float for analysis
      std::vector<float> float_samples(samples);
      for (int i = 0; i < samples; ++i)
      {
        float_samples[i] = convert_to_float(data[i * stride]);
      }

      // Second pass: Detect and fix issues
      for (int i = 0; i < samples; ++i)
      {
        float sample = float_samples[i];

        // Check for NaN or Inf
        bool is_invalid = std::isnan(sample) || std::isinf(sample);

        // Check for high pitch indicators (rapid changes in amplitude)
        bool is_high_pitch = false;
        if (!is_invalid && i >= window_size)
        {
          // Calculate average change over the window
          float avg_change = 0.0f;
          for (int j = 1; j < window_size; ++j)
          {
            avg_change += std::abs(float_samples[i - j] - float_samples[i - j + 1]);
          }
          avg_change /= (window_size - 1);

          // Check if sample has high amplitude AND rapid changes
          if (std::abs(sample) > threshold && avg_change > change_threshold)
          {
            is_high_pitch    = true;
            found_high_pitch = true;
          }
        }

        if (is_invalid)
        {
          LOG_WARNING << TRANSCODER_LOG << "Found invalid sample for data sample: " << sample
                      << " at idx -> " << i;
          sample        = 0.0f;
          found_invalid = true;
        }
        else if (is_high_pitch)
        {
          LOG_WARNING << TRANSCODER_LOG << "Found high pitch sample: " << sample << " at idx -> "
                      << i;
          sample = 0.0f; // Silence the high pitch sample
        }
        else
        {
          sample = clamp(sample);
        }

        // Store the processed sample
        data[i * stride] = convert_from_float(sample);

        // Update previous samples window
        for (int j = window_size - 1; j > 0; --j)
        {
          prev_samples[ch][j] = prev_samples[ch][j - 1];
        }
        prev_samples[ch][0] = sample;
      }
    }

    if (found_invalid)
    {
      LOG_INFO << TRANSCODER_LOG << "Sanitization Job done for invalid samples of format -> "
               << format;
    }

    if (found_high_pitch)
    {
      LOG_INFO << TRANSCODER_LOG
               << "Sanitization Job done for high pitch audio artifacts of format -> " << format;
    }
  }

  inline auto soft_clip(float x) -> float
  {
    return std::tanh(x); // Smoothly compresses extreme values
  }

  inline auto soft_clip(double x) -> double { return std::tanh(x); }

  // Function to sanitize audio samples (to avoid any energy issues from psymodel or quantize source files of libmp3lame)
  void sanitize_audio_samples(AVFrame* frame)
  {
    if (!frame)
      return;
    switch (frame->format)
    {
      case AV_SAMPLE_FMT_FLT:
      case AV_SAMPLE_FMT_FLTP:
        process_samples<float>(
          frame, [](float v) -> float { return v; }, [](float v) -> float { return v; });
        break;
      case AV_SAMPLE_FMT_DBL:
      case AV_SAMPLE_FMT_DBLP:
        process_samples<double>(
          frame, [](double v) -> float { return static_cast<float>(v); },
          [](float v) -> double { return static_cast<double>(v); });
        break;
      case AV_SAMPLE_FMT_S32:
      case AV_SAMPLE_FMT_S32P:
        process_samples<int32_t>(
          frame,
          // int32 to float: multiply by 1/(2^31)
          [](int32_t v) -> float { return static_cast<float>(v) * SCALE_FACTOR_32B; },
          // float to int32: multiply by 2^31
          [](float v) -> int32_t { return static_cast<int32_t>(v * FLOAT_TO_INT32); });
        break;
      case AV_SAMPLE_FMT_S16:
      case AV_SAMPLE_FMT_S16P:
        process_samples<int16_t>(
          frame,
          // int16 to float: multiply by 1/(2^15)
          [](int16_t v) -> float { return static_cast<float>(v) * SCALE_FACTOR_16B; },
          // float to int16: multiply by 2^15
          [](float v) -> int16_t { return static_cast<int16_t>(v * FLOAT_TO_INT16); });
        break;
      default:
        // Unsupported format
        break;
    }
  }
  // Main transcoding function - now more modular
  auto transcode_to_mp3(const char* input_filename, const char* output_filename,
                        const int given_bitrate) -> int
  {
    av_log_set_level(AV_LOG_INFO);

    // Initialize all required contexts and pointers
    int ret                = 0;
    int audio_stream_index = -1;

    try
    {
      // Step 1: Initialize input
      ret = initialize_input(input_filename, &in_format_ctx, &in_codec_ctx, &audio_stream_index);
      if (ret < 0)
      {
        throw std::runtime_error(std::string("Failed to initialize input!"));
      }

      print_audio_info(input_filename, in_format_ctx, in_codec_ctx, "Input File Info");

      // Step 2: Initialize output
      ret = initialize_output(output_filename, &out_format_ctx, &out_codec_ctx, &out_stream,
                              in_codec_ctx, given_bitrate);
      if (ret < 0)
      {
        throw std::runtime_error(std::string("Failed to initialize output!"));
      }

      // Step 3: Initialize resampler
      ret = initialize_resampler(&swr_ctx, in_codec_ctx, out_codec_ctx);
      if (ret < 0)
      {
        throw std::runtime_error(std::string("Failed to initialize resampler!"));
      }

      // Step 4: Prepare frames and packets
      ret = prepare_frames(&frame, &resampled_frame, &packet, out_codec_ctx);
      if (ret < 0)
      {
        throw std::runtime_error(std::string("Failed to prepare frames!"));
      }

      // Step 5: Process audio frames
      ret = process_audio_frames(in_format_ctx, in_codec_ctx, out_codec_ctx, out_format_ctx,
                                 swr_ctx, frame, resampled_frame, packet, audio_stream_index);
      if (ret < 0)
      {
        throw std::runtime_error(std::string("Error processing audio frames!"));
      }

      // Print output file information
      print_audio_info(output_filename, out_format_ctx, out_codec_ctx, "Output File Info");

      LOG_INFO << TRANSCODER_LOG << "==> [Transcoding completed successfully!]";
    }
    catch (const std::exception& e)
    {
      LOG_ERROR << TRANSCODER_LOG << "Transcoding error: " << e.what();
      ret = AVERROR_UNKNOWN;
    }

    // Final cleanup
    cleanup_resources(&in_format_ctx, &out_format_ctx, &in_codec_ctx, &out_codec_ctx, &swr_ctx,
                      &frame, &resampled_frame, &packet);

    return ret;
  }

  // Function to initialize the input file and decoder
  auto initialize_input(const char* input_filename, AVFormatContext** in_format_ctx,
                        AVCodecContext** in_codec_ctx, int* audio_stream_index) -> int
  {
    int ret = 0;

    // Open input file
    if ((ret = avformat_open_input(in_format_ctx, input_filename, nullptr, nullptr)) < 0)
    {
      LOG_ERROR << TRANSCODER_LOG << "Could not open input file!";
      return ret;
    }

    // Read stream information
    if ((ret = avformat_find_stream_info(*in_format_ctx, nullptr)) < 0)
    {
      LOG_ERROR << TRANSCODER_LOG << "Could not find stream info!";
      avformat_close_input(in_format_ctx);
      return ret;
    }

    // Find the audio stream
    *audio_stream_index =
      av_find_best_stream(*in_format_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    if (*audio_stream_index < 0)
    {
      LOG_ERROR << TRANSCODER_LOG << "Could not find an audio stream";
      avformat_close_input(in_format_ctx);
      return AVERROR_STREAM_NOT_FOUND;
    }

    AVStream* in_stream = (*in_format_ctx)->streams[*audio_stream_index];

    // Find decoder for the audio stream
    const AVCodec* in_codec = avcodec_find_decoder(in_stream->codecpar->codec_id);
    if (!in_codec)
    {
      LOG_ERROR << TRANSCODER_LOG
                << "Decoder not found for codec ID: " << in_stream->codecpar->codec_id;
      avformat_close_input(in_format_ctx);
      return AVERROR_DECODER_NOT_FOUND;
    }

    LOG_INFO << "==> Found codec " << in_codec->name << " <==";

    // Allocate and initialize the decoder context
    *in_codec_ctx = avcodec_alloc_context3(in_codec);
    if (!*in_codec_ctx)
    {
      LOG_ERROR << TRANSCODER_LOG << "Could not allocate a decoding context";
      avformat_close_input(in_format_ctx);
      return AVERROR(ENOMEM);
    }

    // Copy parameters from the input stream to the decoder context
    if ((ret = avcodec_parameters_to_context(*in_codec_ctx, in_stream->codecpar)) < 0)
    {
      LOG_ERROR << TRANSCODER_LOG << "Failed to copy codec parameters to decoder context!";
      avcodec_free_context(in_codec_ctx);
      avformat_close_input(in_format_ctx);
      return ret;
    }

    // Set the sample rate and timebase
    (*in_codec_ctx)->pkt_timebase = in_stream->time_base;

    // Open the decoder
    if ((ret = avcodec_open2(*in_codec_ctx, in_codec, nullptr)) < 0)
    {
      LOG_ERROR << TRANSCODER_LOG << "Failed to open decoder!";
      avcodec_free_context(in_codec_ctx);
      avformat_close_input(in_format_ctx);
      return ret;
    }

    return 0;
  }

  // Function to initialize the output file and encoder
  auto initialize_output(const char* output_filename, AVFormatContext** out_format_ctx,
                         AVCodecContext** out_codec_ctx, AVStream** out_stream,
                         AVCodecContext* in_codec_ctx, int bitrate) -> int
  {
    int ret = 0;

    // Create output format context
    if ((ret = avformat_alloc_output_context2(out_format_ctx, nullptr, nullptr, output_filename)) <
        0)
    {
      LOG_ERROR << TRANSCODER_LOG << "Could not create output context!";
      return ret;
    }

    // Find MP3 (LAME) encoder
    const AVCodec* out_codec = avcodec_find_encoder(AV_CODEC_ID_MP3);
    if (!out_codec)
    {
      LOG_ERROR << TRANSCODER_LOG << "MP3 encoder not found";
      avformat_free_context(*out_format_ctx);
      return AVERROR_ENCODER_NOT_FOUND;
    }

    LOG_INFO << TRANSCODER_LOG << "Transcoding using MP3 (libmp3lame, ID: " << AV_CODEC_ID_MP3
             << ")";

    // Create an audio stream in the output file
    *out_stream = avformat_new_stream(*out_format_ctx, nullptr);
    if (!*out_stream)
    {
      LOG_ERROR << TRANSCODER_LOG << "Failed to create output stream";
      avformat_free_context(*out_format_ctx);
      return AVERROR(ENOMEM);
    }

    // Allocate and initialize the encoder context
    *out_codec_ctx = avcodec_alloc_context3(out_codec);
    if (!*out_codec_ctx)
    {
      LOG_ERROR << TRANSCODER_LOG << "Could not allocate encoding context";
      avformat_free_context(*out_format_ctx);
      return AVERROR(ENOMEM);
    }

    // Set encoder parameters
    (*out_codec_ctx)->bit_rate    = bitrate;
    (*out_codec_ctx)->sample_rate = in_codec_ctx->sample_rate;

    // Handle FLAC input sample format conversion
    if (in_codec_ctx->sample_fmt == AV_SAMPLE_FMT_S16 ||
        in_codec_ctx->sample_fmt == AV_SAMPLE_FMT_S32)
    {
      LOG_INFO << TRANSCODER_LOG << "FLAC input detected, converting to MP3-compatible format.";
      (*out_codec_ctx)->sample_fmt = AV_SAMPLE_FMT_FLTP;
    }
    else
    {
      (*out_codec_ctx)->sample_fmt = in_codec_ctx->sample_fmt;
    }

    // Copy channel layout from input
    if ((ret = av_channel_layout_copy(&(*out_codec_ctx)->ch_layout, &in_codec_ctx->ch_layout)) < 0)
    {
      LOG_ERROR << TRANSCODER_LOG << "Failed to copy input channel layout to output!";
      avcodec_free_context(out_codec_ctx);
      avformat_free_context(*out_format_ctx);
      return ret;
    }

    // MP3 has limitations on channel count
    if ((*out_codec_ctx)->ch_layout.nb_channels > 2)
    {
      LOG_DEBUG << TRANSCODER_LOG
                << "Warning: MP3 typically supports only mono or stereo. Limiting to stereo.";
      av_channel_layout_uninit(&(*out_codec_ctx)->ch_layout);
      av_channel_layout_default(&(*out_codec_ctx)->ch_layout, 2);
    }

    // Set time base for encoding (sample rate)
    (*out_codec_ctx)->time_base = (AVRational){1, (*out_codec_ctx)->sample_rate};

    // Set encoder quality
    (*out_codec_ctx)->compression_level = 5; // Medium quality (range is 0-9)

    // Set VBR quality - optional setting for better quality
    av_opt_set_int((*out_codec_ctx)->priv_data, "qscale", 1, 0); // Lower values = higher quality

    LOG_DEBUG << TRANSCODER_LOG << "Input Channels: " << in_codec_ctx->ch_layout.nb_channels;
    LOG_DEBUG << TRANSCODER_LOG << "Output Channels: " << (*out_codec_ctx)->ch_layout.nb_channels;

    // Some formats require global header
    if ((*out_format_ctx)->oformat->flags & AVFMT_GLOBALHEADER)
    {
      (*out_codec_ctx)->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    // Open the encoder
    if ((ret = avcodec_open2(*out_codec_ctx, out_codec, nullptr)) < 0)
    {
      LOG_ERROR << TRANSCODER_LOG << "Could not open output codec!";
      avcodec_free_context(out_codec_ctx);
      avformat_free_context(*out_format_ctx);
      return ret;
    }

    LOG_DEBUG << TRANSCODER_LOG << "==> Opened encoder successfully";

    // Copy encoder parameters to output stream
    if ((ret = avcodec_parameters_from_context((*out_stream)->codecpar, *out_codec_ctx)) < 0)
    {
      LOG_ERROR << TRANSCODER_LOG << "Failed to copy encoder parameters to output stream!";
      avcodec_free_context(out_codec_ctx);
      avformat_free_context(*out_format_ctx);
      return ret;
    }

    // Set output stream time base to match encoder time base
    (*out_stream)->time_base = (*out_codec_ctx)->time_base;

    // Open output file
    if (!((*out_format_ctx)->oformat->flags & AVFMT_NOFILE))
    {
      if ((ret = avio_open(&(*out_format_ctx)->pb, output_filename, AVIO_FLAG_WRITE)) < 0)
      {
        LOG_ERROR << TRANSCODER_LOG << "Could not open output file!";
        avcodec_free_context(out_codec_ctx);
        avformat_free_context(*out_format_ctx);
        return ret;
      }
      LOG_INFO << TRANSCODER_LOG << "==> Opened output file successfully";
    }

    // Write file header
    if ((ret = avformat_write_header(*out_format_ctx, nullptr)) < 0)
    {
      LOG_ERROR << TRANSCODER_LOG << "Error writing format header!";
      if (!((*out_format_ctx)->oformat->flags & AVFMT_NOFILE))
      {
        avio_closep(&(*out_format_ctx)->pb);
      }
      avcodec_free_context(out_codec_ctx);
      avformat_free_context(*out_format_ctx);
      return ret;
    }

    LOG_DEBUG << TRANSCODER_LOG << "==> Successfully wrote format header";

    return 0;
  }

  // Function to initialize the audio resampler
  auto initialize_resampler(SwrContext** swr_ctx, AVCodecContext* in_codec_ctx,
                            AVCodecContext* out_codec_ctx) -> int
  {
    int ret = 0;

    // Initialize resampler context
    *swr_ctx = swr_alloc();
    if (!*swr_ctx)
    {
      return AVERROR(ENOMEM);
    }

    // Configure the resampler
    ret = swr_alloc_set_opts2(swr_ctx, &out_codec_ctx->ch_layout, out_codec_ctx->sample_fmt,
                              out_codec_ctx->sample_rate, &in_codec_ctx->ch_layout,
                              in_codec_ctx->sample_fmt, in_codec_ctx->sample_rate, 0, nullptr);

    if (ret < 0)
    {
      swr_free(swr_ctx);
      return ret;
    }

    // Set additional options for better quality
    av_opt_set_int(*swr_ctx, "dither_method", SWR_DITHER_TRIANGULAR, 0);

    // Initialize the resampler
    if ((ret = swr_init(*swr_ctx)) < 0)
    {
      swr_free(swr_ctx);
      return ret;
    }

    LOG_DEBUG << TRANSCODER_LOG << "==> Initialized resampler successfully";

    return 0;
  }

  // Function to prepare and allocate frames and packets
  auto prepare_frames(AVFrame** frame, AVFrame** resampled_frame, AVPacket** packet,
                      AVCodecContext* out_codec_ctx) -> int
  {
    if (!*packet || !*frame || !*resampled_frame)
    {
      av_frame_free(frame);
      av_frame_free(resampled_frame);
      av_packet_free(packet);
      return AVERROR(ENOMEM);
    }

    // Set up the resampled frame properties
    av_channel_layout_copy(&(*resampled_frame)->ch_layout, &out_codec_ctx->ch_layout);
    (*resampled_frame)->format      = out_codec_ctx->sample_fmt;
    (*resampled_frame)->sample_rate = out_codec_ctx->sample_rate;

    // For MP3, 1152 samples per frame is common (MPEG1 Layer 3)
    if (out_codec_ctx->frame_size)
    {
      (*resampled_frame)->nb_samples = out_codec_ctx->frame_size;
    }
    else
    {
      (*resampled_frame)->nb_samples = 1152;
    }

    // Allocate buffer for the resampled frame
    int ret = av_frame_get_buffer(*resampled_frame, 0);
    if (ret < 0)
    {
      av_frame_free(frame);
      av_frame_free(resampled_frame);
      av_packet_free(packet);
      return ret;
    }

    LOG_DEBUG << TRANSCODER_LOG << "==> Allocated frame buffers successfully";

    return 0;
  }

  // Function to process and transcode audio frames
  auto process_audio_frames(AVFormatContext* in_format_ctx, AVCodecContext* in_codec_ctx,
                            AVCodecContext* out_codec_ctx, AVFormatContext* out_format_ctx,
                            SwrContext* swr_ctx, AVFrame* frame, AVFrame* resampled_frame,
                            AVPacket* packet, int audio_stream_index) -> int
  {
    int     ret               = 0;
    int64_t next_pts          = 0;
    int     samples_sanitized = 0;

    // Process input packets
    while (av_read_frame(in_format_ctx, packet) >= 0)
    {
      if (packet->stream_index == audio_stream_index)
      {
        ret = decode_audio_packet(in_codec_ctx, packet, frame, out_codec_ctx, out_format_ctx,
                                  swr_ctx, resampled_frame, &next_pts, &samples_sanitized);
        if (ret < 0 && ret != AVERROR(EAGAIN) && ret != AVERROR_EOF)
        {
          LOG_WARNING << "\r[WARNING] Error processing packet!";
        }
      }

      av_packet_unref(packet);
    }

    LOG_INFO
      << TRANSCODER_LOG
      << "Total Audio Sanization Job (ASJ) done while processing and decoding audio packets.";

    if (samples_sanitized > 0)
    {
      LOG_DEBUG << TRANSCODER_LOG << "Total frames with sanitized samples: " << samples_sanitized;
    }

    // Flush the resampler (ensure all buffered FLAC samples are output)
    flush_resampler(swr_ctx, resampled_frame, out_codec_ctx, out_format_ctx, &next_pts,
                    &samples_sanitized);

    // Flush the encoder (ensure all encoded MP3 frames are written)
    flush_encoder(out_codec_ctx, out_format_ctx);

    // Write file trailer
    if ((ret = av_write_trailer(out_format_ctx)) < 0)
    {
      LOG_ERROR << TRANSCODER_LOG << "Error writing trailer!";
      return ret;
    }

    return 0;
  }

  // Function to decode an audio packet and process frames
  auto decode_audio_packet(AVCodecContext* in_codec_ctx, AVPacket* packet, AVFrame* frame,
                           AVCodecContext* out_codec_ctx, AVFormatContext* out_format_ctx,
                           SwrContext* swr_ctx, AVFrame* resampled_frame, int64_t* next_pts,
                           int* samples_sanitized) -> int
  {
    int ret = avcodec_send_packet(in_codec_ctx, packet);
    if (ret < 0 && ret != AVERROR(EAGAIN) && ret != AVERROR_EOF)
    {
      LOG_ERROR << "\r[WARNING] Error sending packet to decoder!" << std::flush;
      return ret;
    }

    // Process decoded frames
    while ((ret = avcodec_receive_frame(in_codec_ctx, frame)) == 0)
    {
      // Sanitize audio samples to fix NaN and Inf values
      sanitize_audio_samples(frame);

      ret = resample_and_encode_frame(frame, resampled_frame, swr_ctx, out_codec_ctx,
                                      out_format_ctx, next_pts, samples_sanitized);
      if (ret < 0)
      {
        return ret;
      }
    }

    return (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) ? 0 : ret;
  }

  // Function to resample and encode a single audio frame
  auto resample_and_encode_frame(AVFrame* frame, AVFrame* resampled_frame, SwrContext* swr_ctx,
                                 AVCodecContext* out_codec_ctx, AVFormatContext* out_format_ctx,
                                 int64_t* next_pts, int* samples_sanitized) -> int
  {
    int ret = 0;

    // Prepare the resampled frame for writing
    av_frame_make_writable(resampled_frame);

    // Resample the frame
    int num_samples = swr_convert(swr_ctx, resampled_frame->data, resampled_frame->nb_samples,
                                  (const uint8_t**)frame->data, frame->nb_samples);

    if (num_samples < 0)
    {
      LOG_ERROR << "\r[WARNING] Resampling failed!" << std::flush;
      (*samples_sanitized)++;
      return num_samples;
    }

    // Set the correct pts for the resampled frame
    if (frame->pts != AV_NOPTS_VALUE)
    {
      resampled_frame->pts =
        av_rescale_q(frame->pts, out_codec_ctx->time_base, out_codec_ctx->time_base);
    }
    else
    {
      // If no pts available, generate based on sample count
      resampled_frame->pts = *next_pts;
    }

    *next_pts = resampled_frame->pts + resampled_frame->nb_samples;

    // Double-check resampled frame for invalid values
    sanitize_audio_samples(resampled_frame);

    // Send the resampled frame to the encoder
    ret = encode_audio_frame(resampled_frame, out_codec_ctx, out_format_ctx);
    if (ret < 0)
    {
      (*samples_sanitized)++;
      return ret;
    }

    return 0;
  }

  // Function to encode and write a single audio frame
  auto encode_audio_frame(AVFrame* frame, AVCodecContext* codec_ctx, AVFormatContext* format_ctx)
    -> int
  {
    int ret = avcodec_send_frame(codec_ctx, frame);
    if (ret < 0)
    {
      LOG_ERROR << "\r[WARNING] Error sending frame to encoder!" << std::flush;
      return ret;
    }

    // Get encoded packets
    AVPacket* out_packet = av_packet_alloc();
    while ((ret = avcodec_receive_packet(codec_ctx, out_packet)) == 0)
    {
      // Rescale packet timestamps
      av_packet_rescale_ts(out_packet, codec_ctx->time_base, format_ctx->streams[0]->time_base);
      out_packet->stream_index = 0;

      // Write the packet to the output file
      ret = av_interleaved_write_frame(format_ctx, out_packet);
      if (ret < 0)
      {
        LOG_ERROR << "\r[WARNING] Error writing packet!" << std::flush;
      }
      av_packet_unref(out_packet);
    }
    av_packet_free(&out_packet);

    return (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) ? 0 : ret;
  }

  auto flush_resampler(SwrContext* swr_ctx, AVFrame* resampled_frame, AVCodecContext* out_codec_ctx,
                       AVFormatContext* out_format_ctx, int64_t* next_pts, int* samples_sanitized)
    -> int
  {
    int ret = 0;

    while (true)
    {
      av_frame_make_writable(resampled_frame);

      int num_samples =
        swr_convert(swr_ctx, resampled_frame->data, resampled_frame->nb_samples, nullptr, 0);

      if (num_samples <= 0)
        break; // No more samples left to process

      resampled_frame->pts = *next_pts;
      *next_pts += num_samples;

      sanitize_audio_samples(resampled_frame);

      ret = encode_audio_frame(resampled_frame, out_codec_ctx, out_format_ctx);
      if (ret < 0)
      {
        (*samples_sanitized)++;
        return ret;
      }
    }

    LOG_INFO << TRANSCODER_LOG << "Total Audio Sanization Job (ASJ) done while flushing resampler.";

    return 0;
  }

  // Function to flush encoder and write remaining packets
  auto flush_encoder(AVCodecContext* codec_ctx, AVFormatContext* format_ctx) -> int
  {
    int ret = avcodec_send_frame(codec_ctx, nullptr);
    if (ret < 0 && ret != AVERROR_EOF)
    {
      LOG_ERROR << TRANSCODER_LOG << "Error sending null frame for flushing!";
      return ret;
    }

    AVPacket* out_packet = av_packet_alloc();
    if (!out_packet)
    {
      LOG_ERROR << TRANSCODER_LOG << "Failed to allocate output packet";
      return AVERROR(ENOMEM);
    }

    while ((ret = avcodec_receive_packet(codec_ctx, out_packet)) == 0)
    {
      // Rescale packet timestamps
      av_packet_rescale_ts(out_packet, codec_ctx->time_base, format_ctx->streams[0]->time_base);
      out_packet->stream_index = 0;

      // Write the packet
      ret = av_interleaved_write_frame(format_ctx, out_packet);
      if (ret < 0)
      {
        LOG_ERROR << TRANSCODER_LOG << "Error writing flushed packet!";
        av_packet_unref(out_packet);
        av_packet_free(&out_packet);
        return ret;
      }

      av_packet_unref(out_packet);
    }

    av_packet_free(&out_packet);

    // Return 0 for both EAGAIN and EOF since EOF is expected behavior
    return (ret == AVERROR_EOF || ret == AVERROR(EAGAIN)) ? 0 : ret;
  }

  // Function to clean up and free all allocated resources
  void cleanup_resources(AVFormatContext** in_format_ctx, AVFormatContext** out_format_ctx,
                         AVCodecContext** in_codec_ctx, AVCodecContext** out_codec_ctx,
                         SwrContext** swr_ctx, AVFrame** frame, AVFrame** resampled_frame,
                         AVPacket** packet)
  {
    if (*in_codec_ctx)
    {
      avcodec_free_context(in_codec_ctx);
    }

    if (*out_codec_ctx)
    {
      avcodec_free_context(out_codec_ctx);
    }

    if (*in_format_ctx)
    {
      avformat_close_input(in_format_ctx);
    }

    if (*out_format_ctx)
    {
      if (!((*out_format_ctx)->oformat->flags & AVFMT_NOFILE))
      {
        avio_closep(&(*out_format_ctx)->pb);
      }
      avformat_free_context(*out_format_ctx);
      *out_format_ctx = nullptr;
    }

    if (*swr_ctx)
    {
      swr_free(swr_ctx);
    }

    if (*frame)
    {
      av_frame_free(frame);
    }

    if (*resampled_frame)
    {
      av_frame_free(resampled_frame);
    }

    if (*packet)
    {
      av_packet_free(packet);
    }
  }
};

} // namespace libwavy::ffmpeg
