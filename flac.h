#ifndef AUDIO_DECODER_H
#define AUDIO_DECODER_H

#include <iostream>
#include <vector>
#include <thread>
#include <cstring>

#define MINIAUDIO_IMPLEMENTATION
#include "include/miniaudio.h"

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
}

/**************************************************************** 
 *
 * @FLAC:
 *
 * This header loads a concatenated flac stream of:
 *
 * init.mp4 + <all-m4s-files-of-bitrate-x>
 *
 * Decodes it by reading the concatenated stream from a file,
 * and loads into miniaudio for playback.
 *
 * This works standalone but while trying to integrate it with
 * decode.hpp and playback.hpp headers is not working.
 *
 ****************************************************************/

// Global audio buffer
std::vector<unsigned char> audioBuffer;

// Check if a codec is lossless
inline bool is_lossless_codec(AVCodecID codec_id) {
    return (codec_id == AV_CODEC_ID_FLAC || codec_id == AV_CODEC_ID_ALAC || codec_id == AV_CODEC_ID_WAVPACK);
}

// Print audio file metadata
inline void print_audio_metadata(AVFormatContext* formatCtx, AVCodecParameters* codecParams, int audioStreamIndex) {
    std::cout << "Audio File Metadata\n";
    std::cout << "File Name: " << formatCtx->url << "\n";
    std::cout << "Codec: " << avcodec_get_name(codecParams->codec_id) << "\n";
    std::cout << "Bitrate: " << codecParams->bit_rate / 1000 << " kbps\n";
    std::cout << "Sample Rate: " << codecParams->sample_rate << " Hz\n";
    std::cout << "Channels: " << codecParams->ch_layout.nb_channels << "\n";
    std::cout << "Format: " << formatCtx->iformat->long_name << "\n";

    if (is_lossless_codec(codecParams->codec_id)) {
        std::cout << "Lossless codec detected\n";
    } else {
        std::cout << "Lossy codec detected\n";
    }
}

// Miniaudio callback
inline void data_callback(ma_device* device, void* output, const void* input, ma_uint32 frameCount) {
    static size_t offset = 0;
    size_t bytesToCopy = frameCount * device->playback.channels * 2; // 16-bit audio

    if (offset + bytesToCopy > audioBuffer.size()) {
        bytesToCopy = audioBuffer.size() - offset;
    }

    std::memcpy(output, &audioBuffer[offset], bytesToCopy);
    offset += bytesToCopy;

    // If end reached, stop playback
    if (offset >= audioBuffer.size()) {
        device->onStop(device);
    }

    (void)input;
}

// Decode fMP4 audio using FFmpeg
inline bool decode_audio(const char* filename) {
    avformat_network_init();
    AVFormatContext* formatCtx = nullptr;

    // Open input file
    if (avformat_open_input(&formatCtx, filename, nullptr, nullptr) != 0) {
        std::cerr << "Could not open file: " << filename << "\n";
        return false;
    }

    if (avformat_find_stream_info(formatCtx, nullptr) < 0) {
        std::cerr << "Could not find stream info\n";
        return false;
    }

    // Find audio stream
    int audioStreamIndex = -1;
    AVCodecParameters* codecParams = nullptr;
    for (unsigned int i = 0; i < formatCtx->nb_streams; i++) {
        if (formatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            audioStreamIndex = i;
            codecParams = formatCtx->streams[i]->codecpar;
            break;
        }
    }

    if (audioStreamIndex == -1) {
        std::cerr << "No audio stream found\n";
        return false;
    }

    // Print metadata before decoding
    print_audio_metadata(formatCtx, codecParams, audioStreamIndex);

    // Find decoder
    const AVCodec* codec = avcodec_find_decoder(codecParams->codec_id);
    if (!codec) {
        std::cerr << "Unsupported codec\n";
        return false;
    }

    AVCodecContext* codecCtx = avcodec_alloc_context3(codec);
    avcodec_parameters_to_context(codecCtx, codecParams);
    avcodec_open2(codecCtx, codec, nullptr);

    // Read frames
    AVPacket* packet = av_packet_alloc();
    AVFrame* frame = av_frame_alloc();

    while (av_read_frame(formatCtx, packet) >= 0) {
        if (packet->stream_index == audioStreamIndex) {
            if (avcodec_send_packet(codecCtx, packet) == 0) {
                while (avcodec_receive_frame(codecCtx, frame) == 0) {
                    size_t dataSize = frame->nb_samples * codecCtx->ch_layout.nb_channels * 2; // 16-bit PCM
                    audioBuffer.insert(audioBuffer.end(), frame->data[0], frame->data[0] + dataSize);
                }
            }
        }
        av_packet_unref(packet);
    }

    // Cleanup
    av_frame_free(&frame);
    av_packet_free(&packet);
    avcodec_free_context(&codecCtx);
    avformat_close_input(&formatCtx);

    return true;
}

// Play PCM audio using miniaudio
inline void play_audio() {
    ma_device_config deviceConfig;
    ma_device device;

    deviceConfig = ma_device_config_init(ma_device_type_playback);
    deviceConfig.playback.format   = ma_format_s16;
    deviceConfig.playback.channels = 2;
    deviceConfig.sampleRate        = 44100;
    deviceConfig.dataCallback      = data_callback;

    if (ma_device_init(nullptr, &deviceConfig, &device) != MA_SUCCESS) {
        std::cerr << "Failed to initialize miniaudio device\n";
        return;
    }

    ma_device_start(&device);
    std::cout << "Playing audio...\n";

    // Wait until playback finishes
    while (ma_device_is_started(&device)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    ma_device_uninit(&device);
}

#endif // AUDIO_DECODER_H
