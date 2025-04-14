#pragma once
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

#define MINIAUDIO_IMPLEMENTATION
#include <cstring>
#include <libwavy/audio/interface.hpp>
#include <libwavy/common/macros.hpp>
#include <libwavy/logger.hpp>
#include <libwavy/miniaudio.h>
#include <stdexcept>
#include <vector>

namespace libwavy::audio
{

class MiniAudioBackend : public IAudioBackend
{
private:
  ma_device                  device{};
  ma_decoder                 decoder{};
  std::vector<unsigned char> audioMemory;
  bool                       isPlaying{false};
  bool                       flacStream{false};

  static void flacDataCallback(ma_device* pDevice, void* pOutput, const void*, ma_uint32 frameCount)
  {
    static size_t offset  = 0;
    auto*         backend = (MiniAudioBackend*)pDevice->pUserData;
    size_t        bytesToCopy;

    WAVY__SAFE_MULTIPLY(frameCount, pDevice->playback.channels, bytesToCopy);
    WAVY__SAFE_MULTIPLY(bytesToCopy, ma_get_bytes_per_sample(pDevice->playback.format),
                        bytesToCopy);

    if (offset + bytesToCopy > backend->audioMemory.size())
    {
      bytesToCopy = backend->audioMemory.size() - offset;
    }

    std::memcpy(pOutput, &backend->audioMemory[offset], bytesToCopy);
    offset += bytesToCopy;

    if (offset >= backend->audioMemory.size())
    {
      backend->isPlaying = false;
    }
  }

  static void lossyDataCallback(ma_device* pDevice, void* pOutput, const void*,
                                ma_uint32 frameCount)
  {
    auto* backend = (MiniAudioBackend*)pDevice->pUserData;
    auto* output  = (float*)pOutput;

    ma_uint64 framesRead;
    ma_decoder_read_pcm_frames(&backend->decoder, output, frameCount, &framesRead);

    if (framesRead < frameCount)
    {
      ma_silence_pcm_frames(output + (framesRead * pDevice->playback.channels),
                            frameCount - framesRead, ma_format_f32, pDevice->playback.channels);
      backend->isPlaying = false;
    }
  }

public:
  auto initialize(const std::vector<unsigned char>& audioInput, bool isFlac,
                  int preferredSampleRate, int preferredChannels) -> bool override
  {
    flacStream  = isFlac;
    audioMemory = audioInput;

    ma_decoder_config decoderConfig = ma_decoder_config_init_default();
    if (ma_decoder_init_memory(audioMemory.data(), audioMemory.size(), &decoderConfig, &decoder) !=
        MA_SUCCESS)
    {
      LOG_ERROR << AUDIO_LOG << "Decoder init failed.";
      return false;
    }

    decoderConfig.format   = decoder.outputFormat;
    decoderConfig.channels = (preferredChannels > 0) ? preferredChannels : decoder.outputChannels;
    decoderConfig.sampleRate =
      (preferredSampleRate > 0) ? preferredSampleRate : decoder.outputSampleRate;

    ma_device_config config  = ma_device_config_init(ma_device_type_playback);
    config.pUserData         = this;
    config.sampleRate        = decoderConfig.sampleRate;
    config.playback.channels = decoderConfig.channels;

    switch (decoder.outputFormat)
    {
      case ma_format_s16:
      case ma_format_s24:
        config.playback.format = decoder.outputFormat;
        config.dataCallback    = flacDataCallback;
        break;
      case ma_format_f32:
        config.playback.format = decoder.outputFormat;
        config.dataCallback    = (flacStream ? flacDataCallback : lossyDataCallback);
        break;
      default:
        config.playback.format = ma_format_s16;
        config.dataCallback    = flacDataCallback;
        break;
    }

    if (ma_device_init(nullptr, &config, &device) != MA_SUCCESS)
    {
      LOG_ERROR << AUDIO_LOG << "Device init failed.";
      ma_decoder_uninit(&decoder);
      return false;
    }

    return true;
  }

  void play() override
  {
    isPlaying = true;
    if (ma_device_start(&device) != MA_SUCCESS)
    {
      LOG_ERROR << AUDIO_LOG << "Failed to start device.";
      throw std::runtime_error("Device start failed");
    }

    while (isPlaying)
    {
      ma_sleep(100);
    }
  }

  [[nodiscard]] auto name() const -> const char* override { return "MiniAudio Plugin Backend"; }

  ~MiniAudioBackend() override
  {
    LOG_INFO << AUDIO_LOG << "Cleaning up MiniAudioBackend.";
    ma_device_uninit(&device);
    ma_decoder_uninit(&decoder);
  }
};

} // namespace libwavy::audio
