#include <iostream>
extern "C" {
    #include <libavformat/avformat.h>
    #include <libavcodec/avcodec.h>
    #include <libavutil/opt.h>
    #include <libswresample/swresample.h>
}

void convert_to_mp3(const char* input_playlist, const char* output_file) {
    AVFormatContext* input_ctx = nullptr;
    AVFormatContext* output_ctx = nullptr;
    AVCodecContext* decoder_ctx = nullptr;
    AVCodecContext* encoder_ctx = nullptr;
    AVStream* input_stream = nullptr;
    AVStream* output_stream = nullptr;
    int audio_stream_index = -1;

    avformat_network_init();
    av_log_set_level(AV_LOG_DEBUG);

    // Open HLS playlist (.m3u8)
    if (avformat_open_input(&input_ctx, input_playlist, nullptr, nullptr) < 0) {
        av_log(nullptr, AV_LOG_ERROR, "Failed to open input file: %s\n", input_playlist);
        return;
    }

    // Retrieve stream info
    if (avformat_find_stream_info(input_ctx, nullptr) < 0) {
        av_log(nullptr, AV_LOG_ERROR, "Failed to retrieve stream info\n");
        avformat_close_input(&input_ctx);
        return;
    }

    // Find the first audio stream
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

    input_stream = input_ctx->streams[audio_stream_index];

    // Find decoder for input audio stream
    const AVCodec* decoder = avcodec_find_decoder(input_stream->codecpar->codec_id);
    if (!decoder) {
        av_log(nullptr, AV_LOG_ERROR, "Decoder not found\n");
        avformat_close_input(&input_ctx);
        return;
    }

    decoder_ctx = avcodec_alloc_context3(decoder);
    if (!decoder_ctx) {
        av_log(nullptr, AV_LOG_ERROR, "Failed to allocate decoder context\n");
        avformat_close_input(&input_ctx);
        return;
    }

    if (avcodec_parameters_to_context(decoder_ctx, input_stream->codecpar) < 0) {
        av_log(nullptr, AV_LOG_ERROR, "Failed to copy decoder parameters\n");
        avcodec_free_context(&decoder_ctx);
        avformat_close_input(&input_ctx);
        return;
    }

    if (avcodec_open2(decoder_ctx, decoder, nullptr) < 0) {
        av_log(nullptr, AV_LOG_ERROR, "Failed to open decoder\n");
        avcodec_free_context(&decoder_ctx);
        avformat_close_input(&input_ctx);
        return;
    }

    // Allocate output format context
    if (avformat_alloc_output_context2(&output_ctx, nullptr, "mp3", output_file) < 0) {
        av_log(nullptr, AV_LOG_ERROR, "Failed to allocate output context\n");
        avcodec_free_context(&decoder_ctx);
        avformat_close_input(&input_ctx);
        return;
    }

    // Find MP3 encoder
    const AVCodec* encoder = avcodec_find_encoder(AV_CODEC_ID_MP3);
    if (!encoder) {
        av_log(nullptr, AV_LOG_ERROR, "MP3 encoder not found\n");
        avcodec_free_context(&decoder_ctx);
        avformat_close_input(&input_ctx);
        avformat_free_context(output_ctx);
        return;
    }

    // Create output audio stream
    output_stream = avformat_new_stream(output_ctx, encoder);
    if (!output_stream) {
        av_log(nullptr, AV_LOG_ERROR, "Failed to create output stream\n");
        avcodec_free_context(&decoder_ctx);
        avformat_close_input(&input_ctx);
        avformat_free_context(output_ctx);
        return;
    }

    // Allocate encoder context
    encoder_ctx = avcodec_alloc_context3(encoder);
    if (!encoder_ctx) {
        av_log(nullptr, AV_LOG_ERROR, "Failed to allocate encoder context\n");
        return;
    }

    // Set encoder parameters
    encoder_ctx->ch_layout = decoder_ctx->ch_layout;
    encoder_ctx->sample_rate = decoder_ctx->sample_rate;
    encoder_ctx->sample_fmt = AV_SAMPLE_FMT_S16P;  // Use the first supported format
    encoder_ctx->bit_rate = 192000;

    // Open encoder
    if (avcodec_open2(encoder_ctx, encoder, nullptr) < 0) {
        av_log(nullptr, AV_LOG_ERROR, "Failed to open encoder\n");
        avcodec_free_context(&encoder_ctx);
        return;
    }

    // Copy codec parameters to output stream
    if (avcodec_parameters_from_context(output_stream->codecpar, encoder_ctx) < 0) {
        av_log(nullptr, AV_LOG_ERROR, "Failed to copy encoder parameters\n");
        return;
    }

    // Open output file
    if (!(output_ctx->oformat->flags & AVFMT_NOFILE)) {
        if (avio_open(&output_ctx->pb, output_file, AVIO_FLAG_WRITE) < 0) {
            av_log(nullptr, AV_LOG_ERROR, "Failed to open output file: %s\n", output_file);
            return;
        }
    }

    // Write header
    if (avformat_write_header(output_ctx, nullptr) < 0) {
        av_log(nullptr, AV_LOG_ERROR, "Error writing header\n");
        return;
    }

    AVPacket pkt;
    while (av_read_frame(input_ctx, &pkt) >= 0) {
        if (pkt.stream_index == audio_stream_index) {
            int ret = avcodec_send_packet(decoder_ctx, &pkt);
            if (ret < 0) {
                av_log(nullptr, AV_LOG_ERROR, "Error sending packet for decoding: %s\n", av_err2str(ret));
                break;
            }

            AVFrame* frame = av_frame_alloc();
            while (avcodec_receive_frame(decoder_ctx, frame) >= 0) {
                ret = avcodec_send_frame(encoder_ctx, frame);
                if (ret < 0) {
                    av_log(nullptr, AV_LOG_ERROR, "Error sending frame for encoding: %s\n", av_err2str(ret));
                    break;
                }

                AVPacket encoded_pkt;
                av_init_packet(&encoded_pkt);
                ret = avcodec_receive_packet(encoder_ctx, &encoded_pkt);
                if (ret >= 0) {
                    av_interleaved_write_frame(output_ctx, &encoded_pkt);
                    av_packet_unref(&encoded_pkt);
                }
            }

            av_frame_free(&frame);
        }

        av_packet_unref(&pkt);
    }

    // Write trailer and clean up
    av_write_trailer(output_ctx);
    avcodec_free_context(&decoder_ctx);
    avcodec_free_context(&encoder_ctx);
    avformat_close_input(&input_ctx);
    avformat_free_context(output_ctx);
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        av_log(nullptr, AV_LOG_ERROR, "Usage: %s <input.m3u8> <output.mp3>\n", argv[0]);
        return 1;
    }

    convert_to_mp3(argv[1], argv[2]);
    return 0;
}
