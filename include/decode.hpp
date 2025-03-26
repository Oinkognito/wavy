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

auto DBG_WriteDecodedAudioToFile(const std::vector<unsigned char>& transport_segment,
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
  LOG_INFO << "Successfully wrote decoded audio stream to " << filename << std::endl;
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
    LOG_DEBUG << DECODER_LOG << "Audio File Metadata:";
    LOG_DEBUG << DECODER_LOG << "Codec: " << avcodec_get_name(codecParams->codec_id);
    LOG_DEBUG << DECODER_LOG << "Bitrate: " << (double)codecParams->bit_rate / 1000.0 << " kbps";
    LOG_DEBUG << DECODER_LOG << "Sample Rate: " << codecParams->sample_rate << " Hz";
    LOG_DEBUG << DECODER_LOG << "Channels: " << codecParams->ch_layout.nb_channels;
    LOG_DEBUG << DECODER_LOG << "Format: " << formatCtx->iformat->long_name;

    if (is_lossless_codec(codecParams->codec_id))
    {
      LOG_DEBUG << DECODER_LOG << "This is a lossless codec";
    }
    else
    {
      LOG_DEBUG << DECODER_LOG << "This is a lossy codec";
    }
  }

  /**
   * @brief Decodes TS content to raw audio output
   * @param ts_segments Vector of transport stream segments
   * @return true if successful, false otherwise
   */
  bool decode(std::vector<std::string>& ts_segments, std::vector<unsigned char>& output_audio)
  {
    avformat_network_init();
    AVFormatContext* input_ctx = avformat_alloc_context();
    AVIOContext*     avio_ctx  = nullptr;
    int              ret;
    size_t           avio_buf = 32768;

    // Buffer for custom AVIO
    unsigned char* avio_buffer = static_cast<unsigned char*>(av_malloc(avio_buf));
    if (!avio_buffer)
    {
      av_log(nullptr, AV_LOG_ERROR, "Failed to allocate AVIO buffer\n");
      return false;
    }

    // Select appropriate custom reader
    avio_ctx = avio_alloc_context(avio_buffer, avio_buf, 0, &ts_segments, &custom_read_packet,
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

    // Detect format
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
    int                audio_stream_idx = -1;
    AVCodecParameters* codec_params     = nullptr;

    for (unsigned int i = 0; i < input_ctx->nb_streams; i++)
    {
      if (input_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
      {
        audio_stream_idx = i;
        codec_params     = input_ctx->streams[i]->codecpar;
        break;
      }
    }

    if (audio_stream_idx < 0)
    {
      av_log(nullptr, AV_LOG_ERROR, "Cannot find audio stream\n");
      avformat_close_input(&input_ctx);
      return false;
    }

    // Check if the codec is FLAC
    bool is_flac = (codec_params->codec_id == AV_CODEC_ID_FLAC);
    if (is_m4s && is_flac)
    {
      LOG_DEBUG << DECODER_LOG << "Detected FLAC encoding in fragmented MP4 (m4s)";
    }

    print_audio_metadata(input_ctx, codec_params, audio_stream_idx);

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

    if (codec_params->extradata_size > 0)
    {
      codec_ctx->extradata = static_cast<uint8_t*>(
        av_malloc(codec_params->extradata_size + AV_INPUT_BUFFER_PADDING_SIZE));
      memcpy(codec_ctx->extradata, codec_params->extradata, codec_params->extradata_size);
      codec_ctx->extradata_size = codec_params->extradata_size;
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

    // Extract raw audio data
    AVPacket* packet = av_packet_alloc();
    AVFrame*  frame  = av_frame_alloc();

    while (av_read_frame(input_ctx, packet) >= 0)
    {
      if (packet->stream_index == audio_stream_idx)
      {
        if (!is_flac)
        {
          // Directly append MP3/AAC data
          output_audio.insert(output_audio.end(), packet->data, packet->data + packet->size);
        }
        else
        {
          // Decode FLAC to PCM
          if (avcodec_send_packet(codec_ctx, packet) == 0)
          {
            while (avcodec_receive_frame(codec_ctx, frame) == 0)
            {
              int    sampleSize = av_get_bytes_per_sample(codec_ctx->sample_fmt);
              size_t dataSize;
              WAVY__SAFE_MULTIPLY(frame->nb_samples, codec_ctx->ch_layout.nb_channels, dataSize);
              WAVY__SAFE_MULTIPLY(dataSize, sampleSize, dataSize);
              output_audio.insert(output_audio.end(), frame->data[0], frame->data[0] + dataSize);
            }
          }
        }
      }
      av_packet_unref(packet);
    }

    // Debugging: Write PCM output
    if (!DBG_WriteDecodedAudioToFile(output_audio, "final.pcm"))
    {
      LOG_ERROR << "Error writing decoded stream to file";
      return false;
    }

    // Cleanup
    av_frame_free(&frame);
    av_packet_free(&packet);
    avcodec_free_context(&codec_ctx);
    if (input_ctx)
    {
      avformat_close_input(&input_ctx);
    }
    if (avio_ctx && !(input_ctx && input_ctx->pb == avio_ctx))
    { // Ensure it's not freed twice
      avio_context_free(&avio_ctx);
    }

    return true;
  }
};
