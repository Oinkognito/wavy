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

#include <libwavy/ffmpeg/decoder/entry.hpp>

using Decoder = libwavy::log::DECODER;

namespace libwavy::ffmpeg
{

MediaDecoder::MediaDecoder() { avformat_network_init(); }

MediaDecoder::~MediaDecoder() { avformat_network_deinit(); }

auto MediaDecoder::is_lossless_codec(AVCodecID codec_id) -> bool
{
  return (codec_id == AV_CODEC_ID_FLAC || codec_id == AV_CODEC_ID_ALAC ||
          codec_id == AV_CODEC_ID_WAVPACK);
}

void MediaDecoder::print_audio_metadata(AVFormatContext* formatCtx, AVCodecParameters* codecParams)
{
  log::DBG<Decoder>("-------------- Audio File Metadata -----------------");
  log::DBG<Decoder>("Codec:           {}", avcodec_get_name(codecParams->codec_id));
  log::DBG<Decoder>("Bitrate:         {} kbps", (double)codecParams->bit_rate / 1000.0);
  log::DBG<Decoder>("Sample Rate:     {} Hz", codecParams->sample_rate);
  log::DBG<Decoder>("Channels:        {}", codecParams->ch_layout.nb_channels);
  log::DBG<Decoder>("Format:          {}", formatCtx->iformat->long_name);
  log::DBG<Decoder>("--> {}", (is_lossless_codec(codecParams->codec_id) ? "This is a lossless codec"
                                                                        : "This is a lossy codec"));
}

auto MediaDecoder::decode(TotalAudioData& ts_segments, TotalDecodedAudioData& output_audio) -> bool
{
  AVFormatContext* input_ctx        = nullptr;
  AVCodecContext*  codec_ctx        = nullptr;
  AudioStreamIdx   audio_stream_idx = -1;

  if (!init_input_context(ts_segments, input_ctx))
    return false;

  detect_format(input_ctx);

  if (!find_audio_stream(input_ctx, audio_stream_idx))
  {
    cleanup(input_ctx);
    return false;
  }

  AVCodecParameters* codec_params = input_ctx->streams[audio_stream_idx]->codecpar;
  print_audio_metadata(input_ctx, codec_params);

  if (!setup_codec(codec_params, codec_ctx))
  {
    cleanup(input_ctx);
    return false;
  }

  if (!process_packets(input_ctx, codec_ctx, audio_stream_idx, codec_params, output_audio))
  {
    cleanup(input_ctx, codec_ctx);
    return false;
  }

#ifdef WAVY_DBG_RUN
  libwavy::dbg::FileWriter<DecodedAudioData>::write(output_audio, "final.pcm");
#endif

  cleanup(input_ctx, codec_ctx);
  return true;
}

auto MediaDecoder::init_input_context(TotalAudioData& ts_segments, AVFormatContext*& ctx) -> bool
{
  ctx          = avformat_alloc_context();
  auto* buffer = static_cast<DecodedAudioData*>(av_malloc(DefaultAVIOBufferSize));
  if (!buffer)
    return false;

  AVIOContext* avio_ctx =
    avio_alloc_context(buffer, DefaultAVIOBufferSize, 0, &ts_segments, &readAVIO, nullptr, nullptr);
  if (!avio_ctx)
    return false;

  ctx->pb = avio_ctx;

  if (avformat_open_input(&ctx, nullptr, nullptr, nullptr) < 0)
    return false;
  if (avformat_find_stream_info(ctx, nullptr) < 0)
    return false;

  return true;
}

void MediaDecoder::detect_format(AVFormatContext* ctx)
{
  std::string fmt = ctx->iformat->name;
  if (fmt == macros::MPEG_TS)
    av_log(nullptr, AV_LOG_DEBUG, "Input is an MPEG transport stream\n");
  else if (fmt.find(macros::MP4_TS) != std::string::npos)
    av_log(nullptr, AV_LOG_DEBUG, "Input is a fragmented MP4 (m4s)\n");
  else
    av_log(nullptr, AV_LOG_WARNING, "Unknown or unsupported format detected\n");
}

auto MediaDecoder::find_audio_stream(AVFormatContext* ctx, AudioStreamIdx& index) -> bool
{
  for (AudioStreamIdxIter i = 0; i < ctx->nb_streams; i++)
  {
    if (ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
    {
      index = i;
      return true;
    }
  }
  av_log(nullptr, AV_LOG_ERROR, "Cannot find audio stream\n");
  return false;
}

auto MediaDecoder::setup_codec(AVCodecParameters* codec_params, AVCodecContext*& codec_ctx) -> bool
{
  const AVCodec* codec = avcodec_find_decoder(codec_params->codec_id);
  if (!codec)
    return false;

  codec_ctx = avcodec_alloc_context3(codec);
  if (!codec_ctx)
    return false;

  if (codec_params->extradata_size > 0)
  {
    codec_ctx->extradata = static_cast<AudioByte*>(
      av_malloc(codec_params->extradata_size + AV_INPUT_BUFFER_PADDING_SIZE));
    memcpy(codec_ctx->extradata, codec_params->extradata, codec_params->extradata_size);
    codec_ctx->extradata_size = codec_params->extradata_size;
  }

  if (avcodec_parameters_to_context(codec_ctx, codec_params) < 0)
    return false;
  if (avcodec_open2(codec_ctx, codec, nullptr) < 0)
    return false;

  return true;
}

auto MediaDecoder::process_packets(AVFormatContext* ctx, AVCodecContext* codec_ctx,
                                   AudioStreamIdx stream_idx, AVCodecParameters* codec_params,
                                   TotalDecodedAudioData& output) -> bool
{
  AVPacket* packet  = av_packet_alloc();
  AVFrame*  frame   = av_frame_alloc();
  bool      is_flac = (codec_params->codec_id == AV_CODEC_ID_FLAC);

  if (!packet || !frame)
  {
    av_log(nullptr, AV_LOG_ERROR, "Failed to allocate packet or frame\n");
    if (packet)
      av_packet_free(&packet);
    if (frame)
      av_frame_free(&frame);
    return false;
  }

  int ret = 0;
  while ((ret = av_read_frame(ctx, packet)) >= 0)
  {
    if (packet->stream_index != stream_idx)
    {
      av_packet_unref(packet);
      continue;
    }

    ret = avcodec_send_packet(codec_ctx, packet);
    if (ret < 0)
    {
      char errbuf[128];
      av_strerror(ret, errbuf, sizeof(errbuf));
      log::ERROR<Decoder>("Error sending packet: {}", errbuf);
      av_packet_unref(packet);
      continue;
    }

    while ((ret = avcodec_receive_frame(codec_ctx, frame)) == 0)
    {
      auto fmt            = static_cast<AVSampleFormat>(frame->format);
      int  channels       = frame->ch_layout.nb_channels;
      int  nb_samples     = frame->nb_samples;
      int  bytesPerSample = av_get_bytes_per_sample(fmt);

      if (av_sample_fmt_is_planar(fmt))
      {
        size_t before_size = output.size();
        for (int i = 0; i < nb_samples; ++i)
        {
          for (int ch = 0; ch < channels; ++ch)
          {
            ui8* src = frame->data[ch] + i * bytesPerSample;
            output.insert(output.end(), src, src + bytesPerSample);
          }
        }

        size_t expected_size = static_cast<size_t>(nb_samples) * channels * bytesPerSample;
        size_t actual_added  = output.size() - before_size;
        if (expected_size != actual_added)
        {
          log::WARN<Decoder>("Size mismatch in planar data: expected {}, got {}", expected_size,
                             actual_added);
        }
      }
      else
      {
        size_t dataSize = static_cast<size_t>(nb_samples) * channels * bytesPerSample;
        if (frame->data[0] && dataSize > 0 && dataSize <= frame->linesize[0])
        {
          size_t before_size = output.size();
          output.insert(output.end(), frame->data[0], frame->data[0] + dataSize);
          size_t actual_added = output.size() - before_size;
          if (actual_added != dataSize)
          {
            log::WARN<Decoder>("Size mismatch in interleaved data: expected {}, got {}", dataSize,
                               actual_added);
          }
        }
      }
    }

    if (ret != AVERROR(EAGAIN) && ret != AVERROR_EOF)
    {
      char errbuf[128];
      av_strerror(ret, errbuf, sizeof(errbuf));
      log::ERROR<Decoder>("Error receiving frame: {}", errbuf);
    }

    av_packet_unref(packet);
  }

  avcodec_send_packet(codec_ctx, nullptr);
  while (avcodec_receive_frame(codec_ctx, frame) == 0)
  {
    auto fmt            = static_cast<AVSampleFormat>(frame->format);
    int  channels       = frame->ch_layout.nb_channels;
    int  nb_samples     = frame->nb_samples;
    int  bytesPerSample = av_get_bytes_per_sample(fmt);

    if (av_sample_fmt_is_planar(fmt))
    {
      for (int i = 0; i < nb_samples; ++i)
      {
        for (int ch = 0; ch < channels; ++ch)
        {
          ui8* src = frame->data[ch] + i * bytesPerSample;
          output.insert(output.end(), src, src + bytesPerSample);
        }
      }
    }
    else
    {
      size_t dataSize = static_cast<size_t>(nb_samples) * channels * bytesPerSample;
      if (frame->data[0] && dataSize > 0 && dataSize <= frame->linesize[0])
      {
        output.insert(output.end(), frame->data[0], frame->data[0] + dataSize);
      }
    }
  }

  av_frame_free(&frame);
  av_packet_free(&packet);

  log::INFO<Decoder>("Decoding complete: {} bytes of raw audio data generated!",
                     libwavy::utils::math::bytesFormat(output.size()));
  log::INFO<Decoder>("Sample format: {}, Bytes per sample: {}",
                     av_get_sample_fmt_name(codec_ctx->sample_fmt),
                     av_get_bytes_per_sample(codec_ctx->sample_fmt));

  return !output.empty();
}

void MediaDecoder::cleanup(AVFormatContext* ctx, AVCodecContext* codec_ctx)
{
  if (codec_ctx)
    avcodec_free_context(&codec_ctx);
  if (ctx)
  {
    if (ctx->pb)
      avio_context_free(&ctx->pb);
    avformat_close_input(&ctx);
  }
}

} // namespace libwavy::ffmpeg
