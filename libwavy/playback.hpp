/************************************************
 * Wavy Project - High-Fidelity Audio Streaming
 * ---------------------------------------------
 * 
 * Copyright (c) 2025 Oinkognito
 * All rights reserved.
 * 
 * This source code is part of the Wavy project, an advanced
 * local networking solution for high-quality audio streaming.
 * 
 * License:
 * This software is licensed under the BSD-3-Clause License.
 * You may use, modify, and distribute this software under
 * the conditions stated in the LICENSE file provided in the
 * project root.
 * 
 * Warranty Disclaimer:
 * This software is provided "AS IS," without any warranties
 * or guarantees, either expressed or implied, including but
 * not limited to fitness for a particular purpose.
 * 
 * Contributions:
 * Contributions to this project are welcome. By submitting 
 * code, you agree to license your contributions under the 
 * same BSD-3-Clause terms.
 * 
 * See LICENSE file for full details.
 ************************************************/

#include <libwavy/common/macros.hpp>
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
#include <cstring>
#include <iomanip> // for std::fixed and std::setprecision
#include <libwavy/logger.hpp>
#include <stdexcept>
#include <vector>

#define SAMPLE_RATE 44100
#define CHANNELS    2

/*****************************************************************
 * @NOTE:
 *
 * MP3 -> No issues
 * FLAC -> s16, s24, s32
 *
 *****************************************************************/

class AudioPlayer
{
private:
  ma_device                  device;
  ma_decoder                 decoder;
  std::vector<unsigned char> audioMemory;
  bool                       isPlaying;
  bool                       flac_stream;

  static void lossyDataCallback(ma_device* pDevice, void* pOutput, const void* pInput,
                                ma_uint32 frameCount)
  {
    auto* player = (AudioPlayer*)pDevice->pUserData;
    auto* output = (float*)pOutput;

    ma_uint64 framesRead;
    ma_decoder_read_pcm_frames(&player->decoder, output, frameCount, &framesRead);

    if (framesRead < frameCount)
    {
      ma_silence_pcm_frames(output + (framesRead * CHANNELS), frameCount - framesRead,
                            ma_format_f32, CHANNELS);
      player->isPlaying = false;
    }
  }

  static void flacDataCallback(ma_device* pDevice, void* pOutput, const void* pInput,
                               ma_uint32 frameCount)
  {
    static size_t offset = 0;
    auto*         player = (AudioPlayer*)pDevice->pUserData;
    size_t        bytesToCopy; // 16-bit FLAC
    WAVY__SAFE_MULTIPLY(frameCount, pDevice->playback.channels, bytesToCopy);
    WAVY__SAFE_MULTIPLY(bytesToCopy, 2, bytesToCopy);

    if (offset + bytesToCopy > player->audioMemory.size())
    {
      bytesToCopy = player->audioMemory.size() - offset;
    }

    std::memcpy(pOutput, &player->audioMemory[offset], bytesToCopy);
    offset += bytesToCopy;

    if (offset >= player->audioMemory.size())
    {
      player->isPlaying = false;
    }

    (void)pInput;
  }

public:
  AudioPlayer(const std::vector<unsigned char>& audioInput, const bool flac_found)
      : audioMemory(audioInput), isPlaying(false), flac_stream(flac_found)
  {
    LOG_INFO << "Initializing AudioPlayer with " << audioMemory.size() << " bytes of audio data.";

    // First, probe the format by initializing the decoder without a predefined format.
    ma_decoder_config decoderConfig = ma_decoder_config_init_default();

    if (ma_decoder_init_memory(audioMemory.data(), audioMemory.size(), &decoderConfig, &decoder) !=
        MA_SUCCESS)
    {
      LOG_ERROR << "Failed to initialize decoder from memory.";
      LOG_WARNING << "Still proceeding to attempt playback...";
    }

    // Dynamically adjust config based on detected format
    decoderConfig.format   = decoder.outputFormat;
    decoderConfig.channels = decoder.outputChannels > 0 ? decoder.outputChannels : CHANNELS;
    decoderConfig.sampleRate =
      decoder.outputSampleRate > 0 ? decoder.outputSampleRate : SAMPLE_RATE;

    LOG_INFO << "Detected Format - Format: " << decoderConfig.format
             << ", Channels: " << decoderConfig.channels
             << ", Sample Rate: " << decoderConfig.sampleRate;

    ma_uint64 totalFrames;
    if (ma_decoder_get_length_in_pcm_frames(&decoder, &totalFrames) == MA_SUCCESS &&
        totalFrames > 0)
    {
      double duration = static_cast<double>(totalFrames) / decoder.outputSampleRate;
      LOG_INFO << "Audio duration: " << std::fixed << std::setprecision(2) << duration
               << " seconds";
    }
    else
    {
      LOG_WARNING << "Could not determine audio duration using standard method.";

      // Estimate duration from raw file size if decoding format is known
      size_t bytesPerSample = (decoder.outputFormat == ma_format_s16)   ? 2
                              : (decoder.outputFormat == ma_format_s24) ? 3
                              : (decoder.outputFormat == ma_format_s32) ? 4
                                                                        : 0;

      if (bytesPerSample > 0 && decoder.outputChannels > 0)
      {
        size_t totalSamples      = audioMemory.size() / (bytesPerSample * decoder.outputChannels);
        double estimatedDuration = static_cast<double>(totalSamples) / decoder.outputSampleRate;

        LOG_INFO << "Estimated Audio Duration: " << std::fixed << std::setprecision(2)
                 << estimatedDuration << " seconds (calculated from file size)";
      }
      else
      {
        LOG_WARNING << "Failed to estimate audio duration.";
      }
    }

    ma_device_config config = ma_device_config_init(ma_device_type_playback);
    config.pUserData        = this;

    // Ensure decoder properties are properly set
    config.playback.channels = decoder.outputChannels > 0 ? decoder.outputChannels : CHANNELS;
    config.sampleRate = decoder.outputSampleRate > 0 ? decoder.outputSampleRate : SAMPLE_RATE;

    // Dynamically determine format based on decoder's output format
    LOG_DEBUG << "[Decoder] " << decoder.outputFormat;
    LOG_DEBUG << "ma_format_s16: " << ma_format_s16;
    LOG_DEBUG << "ma_format_s24: " << ma_format_s24;
    LOG_DEBUG << "ma_format_s32: " << ma_format_s32;
    LOG_DEBUG << "ma_format_f32: " << ma_format_f32;
    switch (decoder.outputFormat)
    {
      case ma_format_s16:
        LOG_INFO << "Using FLAC (16-bit) callback.";
        config.dataCallback    = flacDataCallback;
        config.playback.format = ma_format_s16;
        break;

      case ma_format_s24:
        LOG_INFO << "Using FLAC (24-bit) callback.";
        config.dataCallback    = flacDataCallback;
        config.playback.format = ma_format_s24;
        break;

      case ma_format_f32:
        if (flac_stream)
        {
          LOG_INFO << "Using FLAC (32-bit) callback.";
          config.dataCallback = flacDataCallback;
        }
        else
        {
          LOG_INFO << "Using MP3 (floating-point) callback.";
          config.dataCallback = lossyDataCallback;
        }
        config.playback.format = ma_format_f32;
        break;

      default:
        LOG_WARNING << "Unknown format detected, defaulting to 16-bit FLAC.";
        config.dataCallback    = flacDataCallback;
        config.playback.format = ma_format_s16;
        break;
    }

    LOG_INFO << "Playback Configuration - Format: " << config.playback.format
             << ", Channels: " << config.playback.channels
             << ", Sample Rate: " << config.sampleRate;

    if (ma_device_init(nullptr, &config, &device) != MA_SUCCESS)
    {
      LOG_ERROR << "Failed to initialize audio device.";
      ma_decoder_uninit(&decoder);
      throw std::runtime_error("Failed to initialize audio device");
    }

    LOG_INFO << "Audio device initialized successfully.";
  }

  ~AudioPlayer()
  {
    LOG_INFO << "Shutting down AudioPlayer.";
    ma_device_uninit(&device);
    ma_decoder_uninit(&decoder);
  }

  void play()
  {
    LOG_INFO << "Starting playback.";
    if (ma_device_start(&device) != MA_SUCCESS)
    {
      LOG_ERROR << "Failed to start audio device.";
      throw std::runtime_error("Failed to start audio device");
    }

    isPlaying = true;

    while (isPlaying)
    {
      ma_sleep(100);
    }

    LOG_INFO << "Playback completed.";
  }
};
