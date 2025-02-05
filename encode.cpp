#include <iostream>
extern "C"
{
  #include <libavformat/avformat.h>
  #include <libavcodec/avcodec.h>
  #include <libavutil/opt.h>
}

/**
 * @class HLS_Encoder
 * @brief This class handles the process of encoding an input audio file into HLS segments.
 *
 * The HLS_Encoder class encapsulates the entire process of opening an input file, encoding it to HLS segments,
 * and logging the details of the encoding process.
 */
class HLS_Encoder {
public:
    /**
     * @brief Constructs the encoder object.
     */
    HLS_Encoder() {
        avformat_network_init();
        av_log_set_level(AV_LOG_DEBUG);
    }

    /**
     * @brief Destructs the encoder object.
     */
    ~HLS_Encoder() {
        avformat_network_deinit();
    }

    /**
     * @brief Creates HLS segments from the input audio file and saves the playlist to the specified output file.
     * 
     * This method opens the input audio file, allocates the necessary output context for HLS,
     * copies codec parameters, and writes HLS segments to the output playlist. It also handles
     * packet conversion and logging of the encoding process.
     *
     * @param input_file The path to the input audio file.
     * @param output_playlist The path where the HLS playlist will be saved.
     */
    void create_hls_segments(const char* input_file, const char* output_playlist) {
        AVFormatContext* input_ctx = nullptr;
        AVFormatContext* output_ctx = nullptr;
        AVStream* audio_stream = nullptr;
        int audio_stream_index = -1;
        int64_t total_duration = 0;
        int packet_count = 0;

        if (avformat_open_input(&input_ctx, input_file, nullptr, nullptr) < 0 || !input_ctx) {
            av_log(nullptr, AV_LOG_ERROR, "Failed to open input file: %s\n", input_file);
            return;
        }

        if (avformat_find_stream_info(input_ctx, nullptr) < 0) {
            av_log(nullptr, AV_LOG_ERROR, "Failed to retrieve stream info\n");
            avformat_close_input(&input_ctx);
            return;
        }

        // Find audio stream index
        for (unsigned int i = 0; i < input_ctx->nb_streams; i++) {
            if (input_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
                audio_stream_index = i;
                break;
            }
        }

        if (audio_stream_index == -1) {
            av_log(nullptr, AV_LOG_ERROR, "No audio stream found\n");
            avformat_close_input(&input_ctx);
            return;
        }

        // Allocate output context for HLS
        if (avformat_alloc_output_context2(&output_ctx, nullptr, "hls", output_playlist) < 0 || !output_ctx) {
            av_log(nullptr, AV_LOG_ERROR, "Failed to allocate output context\n");
            avformat_close_input(&input_ctx);
            return;
        }

        // Create new stream for output
        audio_stream = avformat_new_stream(output_ctx, nullptr);
        if (!audio_stream) {
            av_log(nullptr, AV_LOG_ERROR, "Failed to create new stream\n");
            avformat_close_input(&input_ctx);
            avformat_free_context(output_ctx);
            return;
        }

        if (avcodec_parameters_copy(audio_stream->codecpar, input_ctx->streams[audio_stream_index]->codecpar) < 0) {
            av_log(nullptr, AV_LOG_ERROR, "Failed to copy codec parameters\n");
            avformat_close_input(&input_ctx);
            avformat_free_context(output_ctx);
            return;
        }

        // Open output file for writing
        if (!(output_ctx->oformat->flags & AVFMT_NOFILE)) {
            if (avio_open(&output_ctx->pb, output_playlist, AVIO_FLAG_WRITE) < 0 || !output_ctx->pb) {
                av_log(nullptr, AV_LOG_ERROR, "Could not open output file: %s\n", output_playlist);
                avformat_close_input(&input_ctx);
                avformat_free_context(output_ctx);
                return;
            }
        }

        // Set options for HLS
        AVDictionary* options = nullptr;
        av_dict_set(&options, "hls_time", "10", 0);
        av_dict_set(&options, "hls_list_size", "0", 0);
        av_dict_set(&options, "hls_flags", "independent_segments", 0);

        if (avformat_write_header(output_ctx, &options) < 0) {
            av_log(nullptr, AV_LOG_ERROR, "Error occurred while writing header\n");
            avformat_close_input(&input_ctx);
            if (!(output_ctx->oformat->flags & AVFMT_NOFILE)) avio_closep(&output_ctx->pb);
            avformat_free_context(output_ctx);
            return;
        }

        AVPacket pkt;
        while (av_read_frame(input_ctx, &pkt) >= 0) {
            if (pkt.stream_index == audio_stream_index) {
                pkt.stream_index = audio_stream->index;
                packet_count++;
                int64_t original_pts = pkt.pts;
                int64_t original_dts = pkt.dts;
                int64_t original_duration = pkt.duration;

                // Rescale packet timebase
                if (pkt.pts != AV_NOPTS_VALUE)
                    pkt.pts = av_rescale_q(pkt.pts, input_ctx->streams[audio_stream_index]->time_base, audio_stream->time_base);
                if (pkt.dts != AV_NOPTS_VALUE)
                    pkt.dts = av_rescale_q(pkt.dts, input_ctx->streams[audio_stream_index]->time_base, audio_stream->time_base);
                /*
                 * @NOTE:
                 * This ensures that the packet duration is properly scaled when moving from the input stream's time base to the output stream's time base.
                 * Without this conversion, durations could be inconsistent, leading to incorrect segment durations in HLS.
                 */
                if (pkt.duration > 0)
                    pkt.duration = av_rescale_q(pkt.duration, input_ctx->streams[audio_stream_index]->time_base, audio_stream->time_base);

                total_duration += pkt.duration;

                av_log(nullptr, AV_LOG_DEBUG, "Packet #%d - PTS: %ld -> %ld, DTS: %ld -> %ld, Duration: %ld -> %ld\n",
                    packet_count, original_pts, pkt.pts, original_dts, pkt.dts, original_duration, pkt.duration);

                if (av_interleaved_write_frame(output_ctx, &pkt) < 0) {
                    av_log(nullptr, AV_LOG_ERROR, "Error writing frame\n");
                    av_packet_unref(&pkt);
                    break;
                }
            }
            av_packet_unref(&pkt);
        }

        av_log(nullptr, AV_LOG_INFO, "Total packets processed: %d\n", packet_count);
        av_log(nullptr, AV_LOG_INFO, "Total estimated duration: %.2f seconds\n", total_duration * av_q2d(audio_stream->time_base));

        av_write_trailer(output_ctx);

        avformat_close_input(&input_ctx);
        if (!(output_ctx->oformat->flags & AVFMT_NOFILE))
            avio_closep(&output_ctx->pb);
        avformat_free_context(output_ctx);
    }
};

int main(int argc, char* argv[]) {
    if (argc < 3) {
        av_log(nullptr, AV_LOG_ERROR, "Usage: %s <input file> <output playlist.m3u8>\n", argv[0]);
        return 1;
    }

    HLS_Encoder encoder;
    encoder.create_hls_segments(argv[1], argv[2]);

    return 0;
}
