#include "../include/macros.hpp"
#include <fstream>
#include <iostream>

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/audio_fifo.h>
#include <libavutil/opt.h>
}

/**
 * @class TSDecoder
 * @brief Decodes transport stream audio for playback
 *
 * The TSDecoder class handles decoding of transport stream (.ts) files
 * containing audio streams into raw audio output.
 */
class TSDecoder
{
public:
  TSDecoder() { av_log_set_level(AV_LOG_DEBUG); }

  /**
   * @brief Decodes TS content to raw audio output
   * @param input_file Path to the input .ts file
   * @param output_file Path to save the decoded audio
   * @return true if successful, false otherwise
   */
  bool decode_ts(const char* input_file, const char* output_file)
  {
    AVFormatContext* input_ctx  = nullptr;
    AVFormatContext* output_ctx = nullptr;
    int              ret;

    // Open input
    if ((ret = avformat_open_input(&input_ctx, input_file, nullptr, nullptr)) < 0)
    {
      av_log(nullptr, AV_LOG_ERROR, "Cannot open input file\n");
      return false;
    }

    // Get stream information
    if ((ret = avformat_find_stream_info(input_ctx, nullptr)) < 0)
    {
      av_log(nullptr, AV_LOG_ERROR, "Cannot find stream info\n");
      avformat_close_input(&input_ctx);
      return false;
    }

    // Find audio stream
    int audio_stream_idx = av_find_best_stream(input_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    if (audio_stream_idx < 0)
    {
      av_log(nullptr, AV_LOG_ERROR, "Cannot find audio stream\n");
      avformat_close_input(&input_ctx);
      return false;
    }

    // Create output context
    if ((ret = avformat_alloc_output_context2(&output_ctx, nullptr, nullptr, output_file)) < 0)
    {
      av_log(nullptr, AV_LOG_ERROR, "Cannot create output context\n");
      avformat_close_input(&input_ctx);
      return false;
    }

    // Create output stream
    AVStream* out_stream = avformat_new_stream(output_ctx, nullptr);
    if (!out_stream)
    {
      av_log(nullptr, AV_LOG_ERROR, "Cannot create output stream\n");
      avformat_close_input(&input_ctx);
      avformat_free_context(output_ctx);
      return false;
    }

    // Copy codec parameters
    avcodec_parameters_copy(out_stream->codecpar, input_ctx->streams[audio_stream_idx]->codecpar);

    // Open output file
    if ((ret = avio_open(&output_ctx->pb, output_file, AVIO_FLAG_WRITE)) < 0)
    {
      av_log(nullptr, AV_LOG_ERROR, "Cannot open output file\n");
      avformat_close_input(&input_ctx);
      avformat_free_context(output_ctx);
      return false;
    }

    // Write header
    if ((ret = avformat_write_header(output_ctx, nullptr)) < 0)
    {
      av_log(nullptr, AV_LOG_ERROR, "Cannot write header\n");
      avio_closep(&output_ctx->pb);
      avformat_close_input(&input_ctx);
      avformat_free_context(output_ctx);
      return false;
    }

    // Read and write packets
    AVPacket packet;
    while (av_read_frame(input_ctx, &packet) >= 0)
    {
      if (packet.stream_index == audio_stream_idx)
      {
        packet.stream_index = out_stream->index;

        // Rescale timestamps
        packet.pts = av_rescale_q_rnd(
          packet.pts, input_ctx->streams[audio_stream_idx]->time_base, out_stream->time_base,
          static_cast<AVRounding>(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));

        packet.dts = av_rescale_q_rnd(
          packet.dts, input_ctx->streams[audio_stream_idx]->time_base, out_stream->time_base,
          static_cast<AVRounding>(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));

        packet.duration = av_rescale_q(
          packet.duration, input_ctx->streams[audio_stream_idx]->time_base, out_stream->time_base);

        if ((ret = av_interleaved_write_frame(output_ctx, &packet)) < 0)
        {
          av_log(nullptr, AV_LOG_ERROR, "Cannot write frame\n");
          av_packet_unref(&packet);
          break;
        }
      }
      av_packet_unref(&packet);
    }

    // Write trailer
    av_write_trailer(output_ctx);

    // Cleanup
    avio_closep(&output_ctx->pb);
    avformat_close_input(&input_ctx);
    avformat_free_context(output_ctx);

    return true;
  }
};

auto main(int argc, char* argv[]) -> int
{
  if (argc < 3)
  {
    av_log(nullptr, AV_LOG_ERROR, "Usage: %s <input_ts_file> <output_file>\n", argv[0]);
    return 1;
  }

  TSDecoder decoder;

  if (!decoder.decode_ts(argv[1], argv[2]))
  {
    av_log(nullptr, AV_LOG_ERROR, "Decoding failed\n");
    return 1;
  }

  return 0;
}
