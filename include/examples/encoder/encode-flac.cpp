#include <libavutil/samplefmt.h>
extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
}

#include <iostream>

void print_audio_info(const char* filename, AVFormatContext* format_ctx, AVCodecContext* codec_ctx,
                      const char* label)
{
  std::cout << "\n=== " << label << " ===\n";
  std::cout << "File: " << filename << "\n";
  std::cout << "Codec: " << codec_ctx->codec->long_name << " (" << codec_ctx->codec->name << ")\n";
  std::cout << "Bitrate: " << codec_ctx->bit_rate / 1000 << " kbps\n";
  std::cout << "Sample Rate: " << codec_ctx->sample_rate << " Hz\n";
  std::cout << "Channels: " << codec_ctx->ch_layout.nb_channels << "\n";
  std::cout << "Duration: " << format_ctx->duration / AV_TIME_BASE << " sec\n";
  std::cout << "======================\n";
}

void transcode_audio(const char* input_filename, const char* output_filename, AVCodecID codec_id)
{
  av_log_set_level(AV_LOG_INFO);

  AVFormatContext* in_format_ctx = nullptr;
  AVCodecContext*  in_codec_ctx  = nullptr;
  AVCodecContext*  out_codec_ctx = nullptr;
  SwrContext*      swr_ctx       = nullptr;

  // Open input file
  if (avformat_open_input(&in_format_ctx, input_filename, nullptr, nullptr) < 0)
  {
    std::cerr << "Could not open input file\n";
    return;
  }

  if (avformat_find_stream_info(in_format_ctx, nullptr) < 0)
  {
    std::cerr << "Could not find stream info\n";
    return;
  }

  // Find audio stream
  int audio_stream_index =
    av_find_best_stream(in_format_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
  if (audio_stream_index < 0)
  {
    std::cerr << "Could not find an audio stream\n";
    return;
  }

  AVStream*      in_stream = in_format_ctx->streams[audio_stream_index];
  const AVCodec* in_codec  = avcodec_find_decoder(in_stream->codecpar->codec_id);
  if (!in_codec)
  {
    std::cerr << "Decoder not found\n";
    return;
  }

  // Initialize input codec context
  in_codec_ctx = avcodec_alloc_context3(in_codec);
  avcodec_parameters_to_context(in_codec_ctx, in_stream->codecpar);
  avcodec_open2(in_codec_ctx, in_codec, nullptr);

  print_audio_info(input_filename, in_format_ctx, in_codec_ctx, "Input File Info");

  // Initialize output format context
  AVFormatContext* out_format_ctx = nullptr;
  avformat_alloc_output_context2(&out_format_ctx, nullptr, nullptr, output_filename);
  if (!out_format_ctx)
  {
    std::cerr << "Could not create output context\n";
    return;
  }

  const AVCodec* out_codec = avcodec_find_encoder(codec_id);
  if (!out_codec)
  {
    std::cerr << "Encoder not found\n";
    return;
  }

  // Initialize output codec context
  out_codec_ctx              = avcodec_alloc_context3(out_codec);
  out_codec_ctx->bit_rate    = (codec_id == AV_CODEC_ID_MP3) ? 192000 : 128000;
  out_codec_ctx->sample_rate = 44100;
  out_codec_ctx->sample_fmt  = AV_SAMPLE_FMT_FLTP; // Most common sample format
  out_codec_ctx->time_base   = {.num = 1, .den = out_codec_ctx->sample_rate};

  if (in_codec_ctx->ch_layout.nb_channels == 1)
  {
    av_channel_layout_default(&out_codec_ctx->ch_layout, 1); // Mono
  }
  else
  {
    av_channel_layout_default(&out_codec_ctx->ch_layout, 2); // Stereo
  }

  if (avcodec_open2(out_codec_ctx, out_codec, nullptr) < 0)
  {
    std::cerr << "Could not open output codec\n";
    return;
  }

  AVStream* out_stream = avformat_new_stream(out_format_ctx, nullptr);
  avcodec_parameters_from_context(out_stream->codecpar, out_codec_ctx);
  out_stream->time_base = out_codec_ctx->time_base;

  // Open output file
  if (!(out_format_ctx->oformat->flags & AVFMT_NOFILE))
  {
    if (avio_open(&out_format_ctx->pb, output_filename, AVIO_FLAG_WRITE) < 0)
    {
      std::cerr << "Could not open output file\n";
      return;
    }
  }

  if (avformat_write_header(out_format_ctx, nullptr) < 0)
  {
    std::cerr << "Error writing format header\n";
    return;
  }

  // Initialize resampling context
  swr_ctx = swr_alloc();
  int ret = swr_alloc_set_opts2(&swr_ctx, &out_codec_ctx->ch_layout, out_codec_ctx->sample_fmt,
                                out_codec_ctx->sample_rate, &in_codec_ctx->ch_layout,
                                in_codec_ctx->sample_fmt, in_codec_ctx->sample_rate, 0, nullptr);
  if ((ret = swr_init(swr_ctx)) < 0)
  {
    std::cerr << "Failed to initialize SwrContext: " << av_err2str(ret) << "\n";
  }

  // Read, resample, and encode
  AVPacket* packet             = av_packet_alloc();
  AVFrame*  frame              = av_frame_alloc();
  AVFrame*  resampled_frame    = av_frame_alloc();
  resampled_frame->ch_layout   = out_codec_ctx->ch_layout;
  resampled_frame->format      = out_codec_ctx->sample_fmt;
  resampled_frame->sample_rate = out_codec_ctx->sample_rate;
  resampled_frame->nb_samples  = out_codec_ctx->frame_size;
  av_frame_get_buffer(resampled_frame, 0);

  int total_frames  = 0;
  int total_samples = 0;

  while (av_read_frame(in_format_ctx, packet) >= 0)
  {
    if (packet->stream_index == audio_stream_index)
    {
      avcodec_send_packet(in_codec_ctx, packet);
      while (avcodec_receive_frame(in_codec_ctx, frame) == 0)
      {
        av_frame_make_writable(resampled_frame);
        int output_samples =
          swr_convert(swr_ctx, resampled_frame->data, resampled_frame->nb_samples,
                      (const uint8_t**)frame->data, frame->nb_samples);

        resampled_frame->pts =
          av_rescale_q(frame->pts, in_codec_ctx->time_base, out_codec_ctx->time_base);
        std::cout << "\rFrame " << total_frames << " | PTS: " << resampled_frame->pts
                  << " | Samples: " << output_samples << std::flush;

        avcodec_send_frame(out_codec_ctx, resampled_frame);
        AVPacket* out_packet = av_packet_alloc();
        while (avcodec_receive_packet(out_codec_ctx, out_packet) == 0)
        {
          av_interleaved_write_frame(out_format_ctx, out_packet);
          av_packet_unref(out_packet);
        }
        av_packet_free(&out_packet);
      }
    }
    av_packet_unref(packet);
  }

  // **Flush Remaining Frames**
  avcodec_send_packet(in_codec_ctx, nullptr);
  while (avcodec_receive_frame(in_codec_ctx, frame) == 0)
  {
    av_frame_make_writable(resampled_frame);
    swr_convert(swr_ctx, resampled_frame->data, resampled_frame->nb_samples,
                (const uint8_t**)frame->data, frame->nb_samples);

    resampled_frame->pts =
      av_rescale_q(frame->pts, in_codec_ctx->time_base, out_codec_ctx->time_base);

    avcodec_send_frame(out_codec_ctx, resampled_frame);
    AVPacket* out_packet = av_packet_alloc();
    while (avcodec_receive_packet(out_codec_ctx, out_packet) == 0)
    {
      av_interleaved_write_frame(out_format_ctx, out_packet);
      av_packet_unref(out_packet);
    }
    av_packet_free(&out_packet);
  }

  // **Flush the Encoder**
  avcodec_send_frame(out_codec_ctx, nullptr);
  AVPacket* out_packet = av_packet_alloc();
  while (avcodec_receive_packet(out_codec_ctx, out_packet) == 0)
  {
    av_interleaved_write_frame(out_format_ctx, out_packet);
    av_packet_unref(out_packet);
  }
  av_packet_free(&out_packet);

  av_write_trailer(out_format_ctx);

  print_audio_info(output_filename, out_format_ctx, out_codec_ctx, "Output File Info");

  // Cleanup
  av_frame_free(&frame);
  av_frame_free(&resampled_frame);
  av_packet_free(&packet);
  swr_free(&swr_ctx);
  avcodec_free_context(&in_codec_ctx);
  avcodec_free_context(&out_codec_ctx);
  avformat_close_input(&in_format_ctx);
  if (!(out_format_ctx->oformat->flags & AVFMT_NOFILE))
  {
    avio_closep(&out_format_ctx->pb);
  }
  avformat_free_context(out_format_ctx);

  std::cout << "Transcoding complete!\n";
}

auto main() -> int
{
  const char* input_file = "../../../sample.flac";
  const char* output_aac = "output.aac";
  const char* output_mp3 = "output.mp3";

  std::cout << "Converting to AAC...\n";
  transcode_audio(input_file, output_aac, AV_CODEC_ID_AAC);

  std::cout << "\nConverting to MP3...\n";
  transcode_audio(input_file, output_mp3, AV_CODEC_ID_MP3);

  return 0;
}
