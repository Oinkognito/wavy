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

/*****************************************************************
 * @NOTE:
 *
 * MP3 -> No issues
 * FLAC -> s16, s24, s32 (works for all)
 *
 *****************************************************************/

namespace libwavy::audio
{

class AudioPlayer
{
private:
  ma_device                  device;
  ma_decoder                 decoder;
  std::vector<unsigned char> audioMemory;
  bool                       isPlaying;
  bool                       flac_stream;
  int                        userSampleRate;
  int                        userChannels;

  static void lossyDataCallback(ma_device* pDevice, void* pOutput, const void* pInput,
                                ma_uint32 frameCount)
  {
    auto* player = (AudioPlayer*)pDevice->pUserData;
    auto* output = (float*)pOutput;

    ma_uint64 framesRead;
    ma_decoder_read_pcm_frames(&player->decoder, output, frameCount, &framesRead);

    if (framesRead < frameCount)
    {
      ma_silence_pcm_frames(output + (framesRead * pDevice->playback.channels),
                            frameCount - framesRead, ma_format_f32, pDevice->playback.channels);
      player->isPlaying = false;
    }
  }

  static void flacDataCallback(ma_device* pDevice, void* pOutput, const void* pInput,
                               ma_uint32 frameCount)
  {
    static size_t offset = 0;
    auto*         player = (AudioPlayer*)pDevice->pUserData;
    size_t        bytesToCopy;

    WAVY__SAFE_MULTIPLY(frameCount, pDevice->playback.channels, bytesToCopy);
    WAVY__SAFE_MULTIPLY(bytesToCopy, ma_get_bytes_per_sample(pDevice->playback.format),
                        bytesToCopy);

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
  AudioPlayer(const std::vector<unsigned char>& audioInput, const bool flac_found,
              int preferredSampleRate = 0, int preferredChannels = 0)
      : audioMemory(audioInput), isPlaying(false), flac_stream(flac_found),
        userSampleRate(preferredSampleRate), userChannels(preferredChannels)
  {
    LOG_INFO << AUDIO_LOG << "Initializing AudioPlayer with " << audioMemory.size() << " bytes of audio data.";
    LOG_DEBUG << AUDIO_LOG << "User preferences - Sample Rate: " << userSampleRate
             << ", Channels: " << userChannels;

    ma_decoder_config decoderConfig = ma_decoder_config_init_default();

    if (ma_decoder_init_memory(audioMemory.data(), audioMemory.size(), &decoderConfig, &decoder) !=
        MA_SUCCESS)
    {
      LOG_ERROR << AUDIO_LOG << "Failed to initialize decoder from memory.";
      LOG_WARNING << AUDIO_LOG << "Still proceeding to attempt playback...";
    }

    decoderConfig.format     = decoder.outputFormat;
    decoderConfig.channels   = (userChannels > 0) ? userChannels : decoder.outputChannels;
    decoderConfig.sampleRate = (userSampleRate > 0) ? userSampleRate : decoder.outputSampleRate;

    LOG_DEBUG << AUDIO_LOG << "Detected Format - Format: " << decoder.outputFormat
             << ", Channels: " << decoder.outputChannels
             << ", Sample Rate: " << decoder.outputSampleRate;

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
      size_t bytesPerSample = ma_get_bytes_per_sample(decoder.outputFormat);
      if (bytesPerSample > 0 && decoder.outputChannels > 0)
      {
        size_t totalSamples      = audioMemory.size() / (bytesPerSample * decoder.outputChannels);
        double estimatedDuration = static_cast<double>(totalSamples) / decoder.outputSampleRate;

        LOG_INFO << AUDIO_LOG << "Estimated Duration: " << std::fixed << std::setprecision(2)
                 << estimatedDuration << " seconds (calculated from file size)";
      }
      else
      {
        LOG_WARNING << AUDIO_LOG << "Failed to estimate duration.";
      }
    }

    ma_device_config config = ma_device_config_init(ma_device_type_playback);
    config.pUserData        = this;

    config.sampleRate        = (userSampleRate > 0) ? userSampleRate : decoder.outputSampleRate;
    config.playback.channels = (userChannels > 0) ? userChannels : decoder.outputChannels;

    switch (decoder.outputFormat)
    {
      case ma_format_s16:
        LOG_INFO << AUDIO_LOG << "Using FLAC (16-bit) callback.";
        config.dataCallback    = flacDataCallback;
        config.playback.format = ma_format_s16;
        break;
      case ma_format_s24:
        LOG_INFO << AUDIO_LOG << "Using FLAC (24-bit) callback.";
        config.dataCallback    = flacDataCallback;
        config.playback.format = ma_format_s24;
        break;
      case ma_format_f32:
        if (flac_stream)
        {
          LOG_INFO << AUDIO_LOG << "Using FLAC (32-bit) callback.";
          config.dataCallback = flacDataCallback;
        }
        else
        {
          LOG_INFO << AUDIO_LOG << "Using MP3 (floating-point) callback.";
          config.dataCallback = lossyDataCallback;
        }
        config.playback.format = ma_format_f32;
        break;
      default:
        LOG_WARNING << AUDIO_LOG << "Unknown format detected, defaulting to FLAC 16-bit.";
        config.dataCallback    = flacDataCallback;
        config.playback.format = ma_format_s16;
        break;
    }

    LOG_TRACE << AUDIO_LOG << "Playback Config - Format: " << config.playback.format
             << ", Channels: " << config.playback.channels
             << ", Sample Rate: " << config.sampleRate;

    if (ma_device_init(nullptr, &config, &device) != MA_SUCCESS)
    {
      LOG_ERROR << "Failed to initialize audio device.";
      ma_decoder_uninit(&decoder);
      throw std::runtime_error("Audio device initialization failed");
    }

    LOG_INFO << "Audio device initialized successfully.";
  }

  ~AudioPlayer()
  {
    LOG_INFO << AUDIO_LOG << "Shutting down AudioPlayer.";
    ma_device_uninit(&device);
    ma_decoder_uninit(&decoder);
  }

  void play()
  {
    LOG_INFO << AUDIO_LOG << "Starting playback.";
    if (ma_device_start(&device) != MA_SUCCESS)
    {
      LOG_ERROR << AUDIO_LOG << "Failed to start audio device.";
      throw std::runtime_error("Audio device start failed");
    }

    isPlaying = true;

    while (isPlaying)
    {
      ma_sleep(100);
    }

    LOG_INFO << "Playback completed.";
  }
};

} // namespace libwavy::audio
