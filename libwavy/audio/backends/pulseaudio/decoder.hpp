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

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/channel_layout.h>
#include <libavutil/opt.h>
#include <libavutil/samplefmt.h>
#include <libswresample/swresample.h>
}

#include <libwavy/logger.hpp>
#include <vector>

class FFmpegDecoder
{
public:
  static auto decodeToPCM(const std::vector<unsigned char>& input, int targetSampleRate,
                          int targetChannels, std::vector<unsigned char>& outPCM) -> bool
  {
    AVFormatContext* fmtCtx   = nullptr;
    AVIOContext*     avioCtx  = nullptr;
    AVCodecContext*  codecCtx = nullptr;
    SwrContext*      swrCtx   = nullptr;
    AVPacket*        packet   = av_packet_alloc();
    AVFrame*         frame    = av_frame_alloc();
    uint8_t*         buffer   = nullptr;

    auto cleanup = [&]()
    {
      if (buffer)
        av_free(buffer);
      if (swrCtx)
        swr_free(&swrCtx);
      if (codecCtx)
        avcodec_free_context(&codecCtx);
      if (fmtCtx)
        avformat_close_input(&fmtCtx);
      if (packet)
        av_packet_free(&packet);
      if (frame)
        av_frame_free(&frame);
    };

    auto bufferSize = input.size();
    buffer          = (uint8_t*)av_memdup(input.data(), bufferSize);
    if (!buffer)
      return false;

    auto* avioBuffer = (uint8_t*)av_malloc(bufferSize + AV_INPUT_BUFFER_PADDING_SIZE);
    if (!avioBuffer)
      return false;
    memcpy(avioBuffer, buffer, bufferSize);

    auto* avioCtxInternal =
      avio_alloc_context(avioBuffer, bufferSize, 0, nullptr, nullptr, nullptr, nullptr);
    fmtCtx     = avformat_alloc_context();
    fmtCtx->pb = avioCtxInternal;

    if (avformat_open_input(&fmtCtx, nullptr, nullptr, nullptr) < 0)
    {
      cleanup();
      return false;
    }

    if (avformat_find_stream_info(fmtCtx, nullptr) < 0)
    {
      cleanup();
      return false;
    }

    const AVCodec* codec       = nullptr;
    int            streamIndex = av_find_best_stream(fmtCtx, AVMEDIA_TYPE_AUDIO, -1, -1, &codec, 0);
    if (streamIndex < 0)
    {
      cleanup();
      return false;
    }

    codecCtx = avcodec_alloc_context3(codec);
    avcodec_parameters_to_context(codecCtx, fmtCtx->streams[streamIndex]->codecpar);
    if (avcodec_open2(codecCtx, codec, nullptr) < 0)
    {
      cleanup();
      return false;
    }

    swrCtx = swr_alloc();

    av_opt_set_int(swrCtx, "in_sample_rate", codecCtx->sample_rate, 0);
    av_opt_set_sample_fmt(swrCtx, "in_sample_fmt", codecCtx->sample_fmt, 0);

    av_opt_set_int(swrCtx, "out_sample_rate", targetSampleRate, 0);
    av_opt_set_sample_fmt(swrCtx, "out_sample_fmt", AV_SAMPLE_FMT_FLT, 0);

    if (swr_init(swrCtx) < 0)
    {
      LOG_ERROR << "Failed to initialize SwrContext";
      return false;
    }

    while (av_read_frame(fmtCtx, packet) >= 0)
    {
      if (packet->stream_index != streamIndex)
      {
        av_packet_unref(packet);
        continue;
      }

      if (avcodec_send_packet(codecCtx, packet) < 0)
      {
        av_packet_unref(packet);
        continue;
      }

      while (avcodec_receive_frame(codecCtx, frame) == 0)
      {
        uint8_t* outBuffer  = nullptr;
        int      outSamples = swr_convert(swrCtx, &outBuffer, frame->nb_samples,
                                          (const uint8_t**)frame->data, frame->nb_samples);

        size_t size = outSamples * targetChannels * sizeof(float);
        outPCM.insert(outPCM.end(), outBuffer, outBuffer + size);
        av_free(outBuffer);
      }

      av_packet_unref(packet);
    }

    cleanup();
    return true;
  }
};
