#include <fstream>
#include <iostream>
#include <vector>
#include "../include/macros.hpp"

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
}

/**
 * @class HLS_Encoder
 * @brief Encodes an input audio file into HLS segments with Adaptive Bitrate (ABR) support.
 *
 * The HLS_Encoder class handles the process of encoding an input audio file into multiple
 * HLS segments, supporting multiple bitrates for Adaptive Bitrate (ABR) streaming.
 *
 * ## **Concepts**
 * - **HLS (HTTP Live Streaming)**: A protocol used for streaming media over the web.
 * - **ABR (Adaptive Bitrate Streaming)**: A technique where multiple bitrate versions of a media
 * file are created, allowing the player to switch based on network conditions.
 * - **M3U8 Playlist**: A playlist format used by HLS, consisting of a **master playlist** and
 * **variant playlists**.
 *
 * ## **How It Works**
 * - The class extracts the audio stream from the input file.
 * - It transcodes the audio to different bitrates.
 * - Each bitrate has its own HLS playlist.
 * - A **master playlist** (`index.m3u8`) is generated, referencing all variant playlists.
 */
class HLS_Encoder
{
public:
  /**
   * @brief Initializes the HLS encoder.
   *
   * This constructor initializes the FFmpeg network stack and sets the log level for debugging.
   */
  HLS_Encoder()
  {
    avformat_network_init();
    av_log_set_level(AV_LOG_DEBUG);
  }

  /**
   * @brief Cleans up resources and deinitializes FFmpeg network stack.
   */
  ~HLS_Encoder() { avformat_network_deinit(); }
  /**
   * @brief Creates HLS segments for multiple bitrates.
   *
   * @param input_file The path to the input audio file.
   * @param bitrates A vector of bitrates (in kbps) for encoding.
   * @param output_dir The directory where HLS playlists and segments will be stored.
   *
   * This function iterates over the provided bitrates, encoding each into an HLS playlist.
   * It then generates a master playlist linking all variant playlists.
   */
  void create_hls_segments(const char* input_file, const std::vector<int>& bitrates,
                           const char* output_dir)
  {
    std::vector<std::string> playlist_files;

    for (int bitrate : bitrates)
    {
      std::string output_playlist =
        std::string(output_dir) + "/hls_" + std::to_string(bitrate) + macros::to_string(macros::PLAYLIST_EXT);
      playlist_files.push_back(output_playlist);

      if (!encode_variant(input_file, output_playlist.c_str(), bitrate))
      {
        av_log(nullptr, AV_LOG_ERROR, "Encoding failed for bitrate: %d\n", bitrate);
        return;
      }
    }

    create_master_playlist(playlist_files, bitrates, output_dir);
  }

private:
  /**
   * @brief Encodes an audio file into HLS format at a specific bitrate.
   *
   * @param input_file The input audio file.
   * @param output_playlist The output HLS playlist (.m3u8 file).
   * @param bitrate The target bitrate (in kbps).
   * @return `true` on success, `false` on failure.
   *
   * This function extracts the audio stream, sets the encoding bitrate, and writes HLS segments.
   */
  auto encode_variant(const char* input_file, const char* output_playlist, int bitrate) -> bool
  {
    AVFormatContext* input_ctx          = nullptr;
    AVFormatContext* output_ctx         = nullptr;
    AVStream*        audio_stream       = nullptr;
    int              audio_stream_index = -1;

    AVDictionary* options = nullptr;

    if (avformat_open_input(&input_ctx, input_file, nullptr, nullptr) < 0)
    {
      av_log(nullptr, AV_LOG_ERROR, "Failed to open input file: %s\n", input_file);
      return false;
    }

    if (avformat_find_stream_info(input_ctx, nullptr) < 0)
    {
      av_log(nullptr, AV_LOG_ERROR, "Failed to retrieve stream info\n");
      avformat_close_input(&input_ctx);
      return false;
    }

    // Find audio stream
    for (unsigned int i = 0; i < input_ctx->nb_streams; i++)
    {
      if (input_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
      {
        audio_stream_index = i;
        break;
      }
    }

    if (audio_stream_index == -1)
    {
      av_log(nullptr, AV_LOG_ERROR, "No audio stream found\n");
      avformat_close_input(&input_ctx);
      return false;
    }

    // Allocate output context for HLS
    if (avformat_alloc_output_context2(&output_ctx, nullptr, "hls", output_playlist) < 0)
    {
      av_log(nullptr, AV_LOG_ERROR, "Failed to allocate output context\n");
      avformat_close_input(&input_ctx);
      return false;
    }

    // Create new stream
    audio_stream = avformat_new_stream(output_ctx, nullptr);
    if (!audio_stream)
    {
      av_log(nullptr, AV_LOG_ERROR, "Failed to create new stream\n");
      avformat_close_input(&input_ctx);
      avformat_free_context(output_ctx);
      return false;
    }

    if (avcodec_parameters_copy(audio_stream->codecpar,
                                input_ctx->streams[audio_stream_index]->codecpar) < 0)
    {
      av_log(nullptr, AV_LOG_ERROR, "Failed to copy codec parameters\n");
      avformat_close_input(&input_ctx);
      avformat_free_context(output_ctx);
      return false;
    }

    // Set bitrate
    audio_stream->codecpar->bit_rate = bitrate * 1000; // Convert kbps to bps

    // Convert output_playlist to std::string
    std::string output_playlist_str = output_playlist;

    // Extract directory from output_playlist
    size_t      last_slash = output_playlist_str.find_last_of('/');
    std::string output_dir =
      (last_slash != std::string::npos) ? output_playlist_str.substr(0, last_slash) : ".";

    // Format segment filename
    std::string segment_filename_format = output_dir + "/hls_" + std::to_string(bitrate) + "_%d.ts";

    av_dict_set(&options, "hls_time", "10", 0);
    av_dict_set(&options, "hls_list_size", "0", 0);
    av_dict_set(&options, "hls_flags", "independent_segments", 0);
    av_dict_set(&options, "hls_segment_filename", segment_filename_format.c_str(), 0);

    // Write header
    if (avformat_write_header(output_ctx, &options) < 0)
    {
      av_log(nullptr, AV_LOG_ERROR, "Error occurred while writing header\n");
      av_dict_free(&options);
      avformat_close_input(&input_ctx);
      avformat_free_context(output_ctx);
      return false;
    }

    // Packet processing loop
    AVPacket pkt;
    while (av_read_frame(input_ctx, &pkt) >= 0)
    {
      if (pkt.stream_index == audio_stream_index)
      {
        pkt.stream_index = audio_stream->index;

        pkt.pts      = av_rescale_q(pkt.pts, input_ctx->streams[audio_stream_index]->time_base,
                                    audio_stream->time_base);
        pkt.dts      = av_rescale_q(pkt.dts, input_ctx->streams[audio_stream_index]->time_base,
                                    audio_stream->time_base);
        pkt.duration = av_rescale_q(pkt.duration, input_ctx->streams[audio_stream_index]->time_base,
                                    audio_stream->time_base);

        if (av_interleaved_write_frame(output_ctx, &pkt) < 0)
        {
          av_log(nullptr, AV_LOG_ERROR, "Error writing frame\n");
          av_packet_unref(&pkt);
          break;
        }
      }
      av_packet_unref(&pkt);
    }

    av_write_trailer(output_ctx);

    av_dict_free(&options);
    avformat_close_input(&input_ctx);
    if (output_ctx)
    {
      if (!(output_ctx->oformat->flags & AVFMT_NOFILE))
        avio_closep(&output_ctx->pb);
      avformat_free_context(output_ctx);
    }

    return true;
  }
  /**
   * @brief Generates the master playlist (.m3u8) linking all variant playlists.
   *
   * @param playlists The list of variant playlists.
   * @param bitrates The corresponding bitrates.
   * @param output_dir The directory to save the master playlist.
   */
  void create_master_playlist(const std::vector<std::string>& playlists,
                              const std::vector<int>& bitrates, const char* output_dir)
  {
    std::string   master_playlist = std::string(output_dir) + "/" + macros::to_string(macros::MASTER_PLAYLIST);
    std::ofstream m3u8(master_playlist);

    if (!m3u8)
    {
      av_log(nullptr, AV_LOG_ERROR, "Failed to create master playlist\n");
      return;
    }

    m3u8 << "#EXTM3U\n";
    m3u8 << "#EXT-X-VERSION:3\n";

    for (size_t i = 0; i < playlists.size(); i++)
    {
      m3u8 << "#EXT-X-STREAM-INF:BANDWIDTH=" << (bitrates[i] * 1000) << ",CODECS=\"mp4a.40.2\"\n";
      m3u8 << playlists[i].substr(strlen(output_dir) + 1) << "\n";
    }

    m3u8.close();
  }
};

auto main(int argc, char* argv[]) -> int
{
  if (argc < 3)
  {
    av_log(nullptr, AV_LOG_ERROR, "Usage: %s <input file> <output directory>\n", argv[0]);
    return 1;
  }

  std::vector<int> bitrates = {64, 128, 256}; // Example bitrates in kbps

  HLS_Encoder encoder;
  encoder.create_hls_segments(argv[1], bitrates, argv[2]);

  return 0;
}
