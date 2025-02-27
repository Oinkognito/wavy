#pragma once

#include "logger.hpp"
#include "macros.hpp"
#include <fstream>
#include <iostream>
#include <vector>

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/audio_fifo.h>
#include <libavutil/mem.h>
#include <libavutil/opt.h>
}

auto DBG_WriteTransportSegmentsToFile(const std::vector<std::string>& transport_segments,
                                      const std::string&              filename) -> bool
{
  std::ofstream output_file(filename, std::ios::binary);
  if (!output_file)
  {
    LOG_ERROR << "Failed to open output file: " << filename;
    return false;
  }

  for (const auto& segment : transport_segments)
  {
    output_file.write(segment.data(), segment.size());
  }

  output_file.close();
  LOG_INFO << "Successfully wrote transport streams to " << filename;
  return true;
}

auto DBG_WriteTransportSegmentsToFile(const std::vector<unsigned char>& transport_segment,
                                      const std::string&                filename) -> bool
{
  std::ofstream output_file(filename, std::ios::binary);
  if (!output_file)
  {
    LOG_ERROR << "Failed to open output file: " << filename << std::endl;
    return false;
  }

  output_file.write(reinterpret_cast<const char*>(transport_segment.data()),
                    transport_segment.size());

  output_file.close();
  LOG_INFO << "Successfully wrote transport stream to " << filename << std::endl;
  return true;
}

// Custom AVIO read function
static int custom_read_packet(void* opaque, uint8_t* buf, int buf_size)
{
  auto*         segments      = static_cast<std::vector<std::string>*>(opaque);
  static size_t segment_index = 0;
  static size_t read_offset   = 0;

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

/**
 * @class MediaDecoder
 * @brief Decodes transport stream audio for playback from a vector
 */
class MediaDecoder
{
public:
  MediaDecoder() {}

  bool is_lossless_codec(AVCodecID codec_id)
  {
    return (codec_id == AV_CODEC_ID_FLAC || codec_id == AV_CODEC_ID_ALAC ||
            codec_id == AV_CODEC_ID_WAVPACK);
  }

  void print_audio_metadata(AVFormatContext* formatCtx, AVCodecParameters* codecParams,
                            int audioStreamIndex)
  {
    LOG_DEBUG << "[Decoder] Audio File Metadata:";
    LOG_DEBUG << "[Decoder] Codec: " << avcodec_get_name(codecParams->codec_id);
    LOG_DEBUG << "[Decoder] Bitrate: " << codecParams->bit_rate / 1000 << " kbps";
    LOG_DEBUG << "[Decoder] Sample Rate: " << codecParams->sample_rate << " Hz";
    LOG_DEBUG << "[Decoder] Channels: " << codecParams->ch_layout.nb_channels;
    LOG_DEBUG << "[Decoder] Format: " << formatCtx->iformat->long_name;

    if (is_lossless_codec(codecParams->codec_id))
    {
      LOG_DEBUG << "[Decoder] This is a lossless codec";
    }
    else
    {
      LOG_DEBUG << "[Decoder] This is a lossy codec";
    }
  }

  /**
   * @brief Decodes TS content to raw audio output
   * @param ts_segments Vector of transport stream segments
   * @return true if successful, false otherwise
   */
  bool decode(const std::vector<std::string>& ts_segments, std::vector<unsigned char>& output_audio)
  {
    avformat_network_init();
    AVFormatContext* input_ctx = avformat_alloc_context();
    AVIOContext*     avio_ctx  = nullptr;
    int              ret;

    // Buffer for custom AVIO
    unsigned char* avio_buffer = static_cast<unsigned char*>(av_malloc(4096));
    if (!avio_buffer)
    {
      av_log(nullptr, AV_LOG_ERROR, "Failed to allocate AVIO buffer\n");
      return false;
    }

    // Create custom AVIOContext
    avio_ctx = avio_alloc_context(avio_buffer, 4096, 0, (void*)&ts_segments, &custom_read_packet,
                                  nullptr, nullptr);
    if (!avio_ctx)
    {
      av_log(nullptr, AV_LOG_ERROR, "Failed to allocate AVIO context\n");
      av_free(avio_buffer);
      return false;
    }

    input_ctx->pb = avio_ctx;

    // Open input
    if ((ret = avformat_open_input(&input_ctx, nullptr, nullptr, nullptr)) < 0)
    {
      av_log(nullptr, AV_LOG_ERROR, "Cannot open input from memory buffer\n");
      av_free(avio_ctx->buffer);
      avio_context_free(&avio_ctx);
      avformat_free_context(input_ctx);
      return false;
    }

    // Get stream information
    if ((ret = avformat_find_stream_info(input_ctx, nullptr)) < 0)
    {
      av_log(nullptr, AV_LOG_ERROR, "Cannot find stream info\n");
      avformat_close_input(&input_ctx);
      return false;
    }

    // Log detected format
    if (input_ctx->iformat && input_ctx->iformat->name)
    {
      av_log(nullptr, AV_LOG_DEBUG, "Detected format: %s\n", input_ctx->iformat->name);
    }
    else
    {
      av_log(nullptr, AV_LOG_WARNING, "Could not detect format\n");
    }

    // Check if input is MPEG-TS or fMP4 (m4s)
    bool is_mpegts = strcmp(input_ctx->iformat->name, "mpegts") == 0;
    bool is_m4s    = strstr(input_ctx->iformat->name, "mp4") != nullptr;

    if (is_mpegts)
    {
      av_log(nullptr, AV_LOG_DEBUG, "Input is an MPEG transport stream\n");
    }
    else if (is_m4s)
    {
      av_log(nullptr, AV_LOG_DEBUG, "Input is a fragmented MP4 (m4s) file\n");
    }
    else
    {
      av_log(nullptr, AV_LOG_WARNING, "Unknown or unsupported format detected\n");
    }

    // Find audio stream
    int audio_stream_idx = av_find_best_stream(input_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    if (audio_stream_idx < 0)
    {
      av_log(nullptr, AV_LOG_ERROR, "Cannot find audio stream\n");
      avformat_close_input(&input_ctx);
      return false;
    }

    AVStream*          audio_stream = input_ctx->streams[audio_stream_idx];
    AVCodecParameters* codec_params = audio_stream->codecpar;

    // Check if the codec is FLAC (for fMP4/m4s streams)
    bool is_flac = (codec_params->codec_id == AV_CODEC_ID_FLAC);
    if (is_m4s || is_flac)
    {
      av_log(nullptr, AV_LOG_INFO, "Detected FLAC encoding in fragmented MP4 (m4s)\n");
    }

    // Open decoder
    const AVCodec* codec = avcodec_find_decoder(codec_params->codec_id);
    if (!codec)
    {
      av_log(nullptr, AV_LOG_ERROR, "Unsupported codec\n");
      avformat_close_input(&input_ctx);
      return false;
    }

    AVCodecContext* codec_ctx = avcodec_alloc_context3(codec);
    if (!codec_ctx)
    {
      av_log(nullptr, AV_LOG_ERROR, "Failed to allocate codec context\n");
      avformat_close_input(&input_ctx);
      return false;
    }

    if ((ret = avcodec_parameters_to_context(codec_ctx, codec_params)) < 0)
    {
      av_log(nullptr, AV_LOG_ERROR, "Failed to copy codec parameters to context\n");
      avcodec_free_context(&codec_ctx);
      avformat_close_input(&input_ctx);
      return false;
    }

    if ((ret = avcodec_open2(codec_ctx, codec, nullptr)) < 0)
    {
      av_log(nullptr, AV_LOG_ERROR, "Failed to open codec\n");
      avcodec_free_context(&codec_ctx);
      avformat_close_input(&input_ctx);
      return false;
    }

    print_audio_metadata(input_ctx, codec_params, audio_stream_idx);

    // Extract raw audio data
    AVPacket* packet = av_packet_alloc();
    AVFrame*  frame  = av_frame_alloc();
    while (av_read_frame(input_ctx, packet) >= 0)
    {
      if (packet->stream_index == audio_stream_idx)
      {
        if (!is_flac)
        {
          output_audio.insert(output_audio.end(), packet->data, packet->data + packet->size);
        }
        else
        {
          if (avcodec_send_packet(codec_ctx, packet) == 0)
          {
            while (avcodec_receive_frame(codec_ctx, frame) == 0)
            {
              size_t dataSize =
                frame->nb_samples * codec_ctx->ch_layout.nb_channels * 2; // 16-bit PCM
              output_audio.insert(output_audio.end(), frame->data[0], frame->data[0] + dataSize);
            }
          }
        }
      }
      av_packet_unref(packet);
    }
    
    // FOR DEBUG PURPOSES
    if (!DBG_WriteTransportSegmentsToFile(output_audio, "final.pcm"))
    {
      LOG_ERROR << "Error writing transport segments to file";
      return false;
    }

    // Cleanup
    av_frame_free(&frame);
    av_packet_free(&packet);
    avcodec_free_context(&codec_ctx);
    avformat_close_input(&input_ctx);

    return true;
  }
};

