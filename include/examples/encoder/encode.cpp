#include <boost/proto/proto_fwd.hpp>
extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libswresample/swresample.h>
}

#include <iostream>

// Sometimes there is some LAME psymodel.c energy error at runtime
//
// `encode_mp3: psymodel.c:576: calc_energy: Assertion `el >= 0' failed.`
//
// This may occur for quite a lot of files, I am not sure why this occurs and how to fix it.

void print_audio_info(const char* filename, AVFormatContext* format_ctx, AVCodecContext* codec_ctx,
                      const char* label)
{
  std::cout << "\n=== " << label << " ===\n";
  std::cout << "File: " << filename << "\n";
  std::cout << "Codec: " << codec_ctx->codec->long_name << " (" << codec_ctx->codec->name << ")\n";
  std::cout << "Bitrate: " << codec_ctx->bit_rate / 1000 << " kbps\n";
  std::cout << "Sample Rate: " << codec_ctx->sample_rate << " Hz\n";
  std::cout << "Channels: " << codec_ctx->ch_layout.nb_channels << "\n";
  std::cout << "Sample Format: " << av_get_sample_fmt_name(codec_ctx->sample_fmt) << "\n";
  std::cout << "Duration: " << format_ctx->duration / AV_TIME_BASE << " sec\n";
  char layout_desc[256];
  av_channel_layout_describe(&codec_ctx->ch_layout, layout_desc, sizeof(layout_desc));
  std::cout << "Channel Layout Description: " << layout_desc << std::endl;
  std::cout << "======================\n";
}

void transcode_mp3(const char* input_filename, const char* output_filename, const int given_bitrate)
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
    for (unsigned int i = 0; i < in_format_ctx->nb_streams; i++)
    {
      if (in_format_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
      {
        audio_stream_index = i;
        break;
      }
    }
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

  const AVCodec* out_codec = avcodec_find_encoder(AV_CODEC_ID_MP3);
  if (!out_codec)
  {
    std::cerr << "Encoder not found\n";
    return;
  }

  std::cout << "-- Encoding using libmp3flame (" << AV_CODEC_ID_MP3 << ")" << std::endl;

  // Initialize output codec context
  out_codec_ctx                        = avcodec_alloc_context3(out_codec);
  out_codec_ctx->bit_rate              = given_bitrate;
  out_codec_ctx->sample_rate           = in_codec_ctx->sample_rate;
  out_codec_ctx->ch_layout.nb_channels = av_channel_layout_check(&out_codec_ctx->ch_layout);
  out_codec_ctx->sample_fmt            = in_codec_ctx->sample_fmt; // MP3 format
  out_codec_ctx->time_base             = out_codec_ctx->time_base;

  // Explicitly set the channel layout for the output codec
  if (av_channel_layout_copy(&out_codec_ctx->ch_layout, &in_codec_ctx->ch_layout) < 0)
  {
    std::cerr << "Failed to copy input channel layout to output\n";
  }

  std::cout << "Input Channels: " << in_codec_ctx->ch_layout.nb_channels << std::endl;
  std::cout << "Output Channels: " << out_codec_ctx->ch_layout.nb_channels << std::endl;

  if (avcodec_open2(out_codec_ctx, out_codec, nullptr) < 0)
  {
    std::cerr << "Could not open output codec\n";
    return;
  }

  std::cout << "-- Opened codec." << std::endl;

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
    std::cout << "-- Opened output file." << std::endl;
  }

  if (avformat_write_header(out_format_ctx, nullptr) < 0)
  {
    std::cerr << "Error writing format header\n";
    return;
  }

  std::cout << "-- Able to write format header!" << std::endl;

  try
  {

    // Allocate the SwrContext before setting options
    swr_ctx = swr_alloc();
    // Set options correctly
    int ret = swr_alloc_set_opts2(&swr_ctx, &out_codec_ctx->ch_layout, out_codec_ctx->sample_fmt,
                                  out_codec_ctx->sample_rate, &in_codec_ctx->ch_layout,
                                  in_codec_ctx->sample_fmt, in_codec_ctx->sample_rate, 0, nullptr);
    // Finally, initialize the resampling context
    if ((ret = swr_init(swr_ctx)) < 0)
    {
      std::cerr << "Failed to initialize SwrContext: " << av_err2str(ret) << "\n";
      return;
    }

    std::cout << "--> Initialized SwrContext!" << std::endl;

    // Read, resample, and encode
    AVPacket* packet          = av_packet_alloc();
    AVFrame*  frame           = av_frame_alloc();
    AVFrame*  resampled_frame = av_frame_alloc();

    if (!packet || !frame || !resampled_frame)
    {
      std::cerr << "Failed to allocate AVPacket or AVFrame!" << std::endl;
    }

    std::cout << "--> Allocated AVPacket and AVFrame.." << std::endl;

    // Ensure out_codec_ctx is initialized
    if (!out_codec_ctx)
    {
      std::cerr << "Output codec context is NULL!" << std::endl;
      return;
    }

    std::cout << "-- Output codec context is good.." << std::endl;

    av_channel_layout_copy(&resampled_frame->ch_layout, &out_codec_ctx->ch_layout);
    resampled_frame->format      = out_codec_ctx->sample_fmt;
    resampled_frame->sample_rate = out_codec_ctx->sample_rate;
    resampled_frame->nb_samples  = out_codec_ctx->frame_size; // May need correction

    // Allocate buffer for frame
    ret = av_frame_get_buffer(resampled_frame, 0);
    if (ret < 0)
    {
      std::cerr << "av_frame_get_buffer() failed: " << av_err2str(ret) << std::endl;
      return;
    }

    std::cout << "--> Allocated buffer for frame!" << std::endl;

    while (av_read_frame(in_format_ctx, packet) >= 0)
    {
      std::cout << "\r[INFO] Read frame: stream_index=" << packet->stream_index << std::flush;

      if (packet->stream_index == audio_stream_index)
      {
        int ret = avcodec_send_packet(in_codec_ctx, packet);
        if (ret < 0)
        {
          std::cerr << "\r[ERROR] Failed to send packet to decoder: " << av_err2str(ret)
                    << std::flush;
          av_packet_unref(packet);
          continue;
        }

        while ((ret = avcodec_receive_frame(in_codec_ctx, frame)) == 0)
        {
          std::cout << "\r[INFO] Decoded PTS=" << frame->pts << " | samples=" << frame->nb_samples
                    << " | format=" << av_get_sample_fmt_name(in_codec_ctx->sample_fmt)
                    << std::flush;

          av_frame_make_writable(resampled_frame);

          int num_samples = swr_convert(swr_ctx, resampled_frame->data, resampled_frame->nb_samples,
                                        (const uint8_t**)frame->data, frame->nb_samples);
          if (num_samples < 0)
          {
            std::cerr << "\r[ERROR] swr_convert failed: " << av_err2str(num_samples) << std::flush;
            continue;
          }

          std::cout << "\r[INFO] Converted samples=" << num_samples << " | format="
                    << av_get_sample_fmt_name((AVSampleFormat)resampled_frame->format)
                    << " | PTS=" << frame->best_effort_timestamp << std::flush;

          resampled_frame->pts =
            av_rescale_q(frame->pts, in_codec_ctx->time_base, out_codec_ctx->time_base);

          ret = avcodec_send_frame(out_codec_ctx, resampled_frame);
          if (ret < 0)
          {
            std::cerr << "\r[ERROR] Failed to send frame to encoder: " << av_err2str(ret)
                      << std::flush;
            continue;
          }

          AVPacket* out_packet = av_packet_alloc();
          while ((ret = avcodec_receive_packet(out_codec_ctx, out_packet)) == 0)
          {
            std::cout << "\r[INFO] Encoded PTS=" << out_packet->pts
                      << " | Size=" << out_packet->size << std::flush;

            ret = av_interleaved_write_frame(out_format_ctx, out_packet);
            if (ret < 0)
            {
              std::cerr << "\r[ERROR] Failed to write packet: " << av_err2str(ret) << std::flush;
            }
            av_packet_unref(out_packet);
          }
          av_packet_free(&out_packet);
        }
      }
      else
      {
        std::cout << "\r[INFO] Skipping non-audio packet, stream_index=" << packet->stream_index
                  << std::flush;
      }

      av_packet_unref(packet);
    }
    std::cout << "Finished processing all frames." << std::endl;

    auto* samples = (float*)resampled_frame->data[0];
    for (int i = 0; i < 10; ++i)
    {
      std::cout << "Sample[" << i << "]: " << samples[i] << "\n";
    }

    // Flush encoder
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

    std::cout << "-- Transcoding complete!\n";
  }
  catch (std::exception& e)
  {
    std::cerr << "Transcoding error: " << e.what() << std::endl;
  }
}

auto main(int argc, char* argv[]) -> int
{
  if (argc == 4)
  {
    const char* input_file    = argv[1];
    const char* output_file   = argv[2];
    const int   given_bitrate = std::stoi(argv[3]);

    transcode_mp3(input_file, output_file, given_bitrate);
  }
  else
  {
    std::cout << argv[0] << " <input-file> " << "<output-file>" << " <bitrate> " << std::endl;
  }
  return 0;
}
