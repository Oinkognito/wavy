#include "../include/macros.hpp"
#include <fstream>
#include <iostream>
#include <vector>
#include <string>
#include <regex>

extern "C" {
    #include <libavcodec/avcodec.h>
    #include <libavformat/avformat.h>
    #include <libavutil/opt.h>
    #include <libavutil/audio_fifo.h>
}

/**
 * @class HLS_Decoder
 * @brief Decodes HLS audio segments for playback
 * 
 * The HLS_Decoder class handles reading and decoding HLS playlists and audio segments.
 * It supports adaptive bitrate streaming by selecting the appropriate quality variant
 * based on the specified target bitrate.
 */
class HLS_Decoder {
public:
    HLS_Decoder() {
        avformat_network_init();
        av_log_set_level(AV_LOG_DEBUG);
    }

    ~HLS_Decoder() {
        avformat_network_deinit();
    }

    /**
     * @brief Decodes HLS content to raw audio output
     * @param master_playlist Path to the master .m3u8 playlist
     * @param output_file Path to save the decoded audio
     * @param target_bitrate Desired bitrate in kbps (will choose closest available)
     * @return true if successful, false otherwise
     */
    bool decode_hls(const char* master_playlist, const char* output_file, int target_bitrate) {
        std::string selected_playlist = select_variant_playlist(master_playlist, target_bitrate);
        if (selected_playlist.empty()) {
            av_log(nullptr, AV_LOG_ERROR, "Failed to select variant playlist\n");
            return false;
        }

        return decode_playlist(selected_playlist.c_str(), output_file);
    }

private:
    struct PlaylistInfo {
        std::string url;
        int bandwidth;
    };

    /**
     * @brief Parses master playlist and selects appropriate variant
     */
    std::string select_variant_playlist(const char* master_playlist, int target_bitrate) {
        std::ifstream file(master_playlist);
        if (!file) {
            av_log(nullptr, AV_LOG_ERROR, "Cannot open master playlist\n");
            return "";
        }

        std::vector<PlaylistInfo> playlists;
        std::string line;
        PlaylistInfo current_info;

        while (std::getline(file, line)) {
            if (line.find("#EXT-X-STREAM-INF:") != std::string::npos) {
                std::regex bandwidth_regex("BANDWIDTH=(\\d+)");
                std::smatch matches;
                if (std::regex_search(line, matches, bandwidth_regex)) {
                    current_info.bandwidth = std::stoi(matches[1]) / 1000; // Convert to kbps
                }
                
                if (std::getline(file, line)) {
                    current_info.url = std::string(master_playlist).substr(
                        0, std::string(master_playlist).find_last_of('/') + 1) + line;
                    playlists.push_back(current_info);
                }
            }
        }

        // Select closest bitrate
        int min_diff = INT_MAX;
        std::string selected_url;
        
        for (const auto& playlist : playlists) {
            int diff = std::abs(playlist.bandwidth - target_bitrate);
            if (diff < min_diff) {
                min_diff = diff;
                selected_url = playlist.url;
            }
        }

        return selected_url;
    }

    /**
     * @brief Decodes a variant playlist and its segments
     */
    bool decode_playlist(const char* playlist_url, const char* output_file) {
        AVFormatContext* input_ctx = nullptr;
        AVFormatContext* output_ctx = nullptr;
        AVAudioFifo* audio_fifo = nullptr;
        int ret;

        // Open input
        if ((ret = avformat_open_input(&input_ctx, playlist_url, nullptr, nullptr)) < 0) {
            av_log(nullptr, AV_LOG_ERROR, "Cannot open input playlist\n");
            return false;
        }

        // Get stream information
        if ((ret = avformat_find_stream_info(input_ctx, nullptr)) < 0) {
            av_log(nullptr, AV_LOG_ERROR, "Cannot find stream info\n");
            avformat_close_input(&input_ctx);
            return false;
        }

        // Find audio stream
        int audio_stream_idx = av_find_best_stream(input_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
        if (audio_stream_idx < 0) {
            av_log(nullptr, AV_LOG_ERROR, "Cannot find audio stream\n");
            avformat_close_input(&input_ctx);
            return false;
        }

        // Create output context
        if ((ret = avformat_alloc_output_context2(&output_ctx, nullptr, nullptr, output_file)) < 0) {
            av_log(nullptr, AV_LOG_ERROR, "Cannot create output context\n");
            avformat_close_input(&input_ctx);
            return false;
        }

        // Create output stream
        AVStream* out_stream = avformat_new_stream(output_ctx, nullptr);
        if (!out_stream) {
            av_log(nullptr, AV_LOG_ERROR, "Cannot create output stream\n");
            avformat_close_input(&input_ctx);
            avformat_free_context(output_ctx);
            return false;
        }

        // Copy codec parameters
        avcodec_parameters_copy(out_stream->codecpar, input_ctx->streams[audio_stream_idx]->codecpar);

        // Open output file
        if ((ret = avio_open(&output_ctx->pb, output_file, AVIO_FLAG_WRITE)) < 0) {
            av_log(nullptr, AV_LOG_ERROR, "Cannot open output file\n");
            avformat_close_input(&input_ctx);
            avformat_free_context(output_ctx);
            return false;
        }

        // Write header
        if ((ret = avformat_write_header(output_ctx, nullptr)) < 0) {
            av_log(nullptr, AV_LOG_ERROR, "Cannot write header\n");
            avio_closep(&output_ctx->pb);
            avformat_close_input(&input_ctx);
            avformat_free_context(output_ctx);
            return false;
        }

        // Read and write packets
        AVPacket packet;
        while (av_read_frame(input_ctx, &packet) >= 0) {
            if (packet.stream_index == audio_stream_idx) {
                packet.stream_index = out_stream->index;
                
                // Rescale timestamps
                packet.pts = av_rescale_q_rnd(packet.pts,
                    input_ctx->streams[audio_stream_idx]->time_base,
                    out_stream->time_base,
                    static_cast<AVRounding>(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
                    
                packet.dts = av_rescale_q_rnd(packet.dts,
                    input_ctx->streams[audio_stream_idx]->time_base,
                    out_stream->time_base,
                    static_cast<AVRounding>(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
                    
                packet.duration = av_rescale_q(packet.duration,
                    input_ctx->streams[audio_stream_idx]->time_base,
                    out_stream->time_base);

                if ((ret = av_interleaved_write_frame(output_ctx, &packet)) < 0) {
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

auto main(int argc, char* argv[]) -> int {
    if (argc < 4) {
        av_log(nullptr, AV_LOG_ERROR, 
            "Usage: %s <master_playlist> <output_file> <target_bitrate_kbps>\n", 
            argv[0]);
        return 1;
    }

    HLS_Decoder decoder;
    int target_bitrate = std::stoi(argv[3]);
    
    if (!decoder.decode_hls(argv[1], argv[2], target_bitrate)) {
        av_log(nullptr, AV_LOG_ERROR, "Decoding failed\n");
        return 1;
    }

    return 0;
}
