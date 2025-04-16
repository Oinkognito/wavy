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
#include <libwavy/miniaudio.h>
#include <libwavy/utils/io/log/entry.hpp>
#include <stdexcept>
#include <vector>

// This is undergoing revamp!!!

namespace libwavy::audio
{

constexpr const char* _AUDIO_BACKEND_TYPE_ = "MiniAudioBackend";

class MiniAudioBackend : public IAudioBackend
{
private:
  ma_device          device{};
  std::vector<float> pcmBuffer; // Raw PCM buffer to hold decoded data
  bool               isPlaying{false};

  // Callback to handle raw PCM playback (direct PCM data)
  static void pcmDataCallback(ma_device* pDevice, void* pOutput, const void*, ma_uint32 frameCount)
  {
    auto* backend = (MiniAudioBackend*)pDevice->pUserData;
    auto* output  = (float*)pOutput;

    size_t framesToCopy    = frameCount * pDevice->playback.channels;
    size_t availableFrames = backend->pcmBuffer.size();

    if (availableFrames < framesToCopy)
    {
      // Not enough data, silence the rest
      std::memcpy(output, backend->pcmBuffer.data(), availableFrames * sizeof(float));
      ma_silence_pcm_frames(output + (availableFrames * pDevice->playback.channels),
                            framesToCopy - availableFrames, ma_format_f32,
                            pDevice->playback.channels);
      backend->isPlaying = false; // End playback if data is exhausted
    }
    else
    {
      // Enough data, just copy it
      std::memcpy(output, backend->pcmBuffer.data(), framesToCopy * sizeof(float));
      backend->pcmBuffer.erase(backend->pcmBuffer.begin(),
                               backend->pcmBuffer.begin() + framesToCopy);
    }
  }

public:
  auto initialize(const std::vector<unsigned char>& audioInput, bool isFlac,
                  int preferredSampleRate, int preferredChannels) -> bool override
  {
    // Check if audio input is valid
    if (audioInput.empty())
    {
      PLUGIN_LOG_ERROR(_AUDIO_BACKEND_TYPE_) << "Audio input is empty.";
      return false;
    }

    // Convert input to PCM (assuming the input is already PCM-encoded)
    size_t pcmSize = audioInput.size() / sizeof(float); // Assuming 32-bit float PCM data
    pcmBuffer.resize(pcmSize);

    // Copy the audio input data into the PCM buffer
    std::memcpy(pcmBuffer.data(), audioInput.data(), audioInput.size());

    // Configure playback device
    ma_device_config config = ma_device_config_init(ma_device_type_playback);
    config.pUserData        = this;
    config.sampleRate = preferredSampleRate > 0 ? preferredSampleRate : 48000; // Default to 48kHz
    config.playback.channels = preferredChannels > 0 ? preferredChannels : 2;  // Default to stereo

    // Set the playback format based on FLAC or MP3 input
    if (isFlac)
    {
      // FLAC typically uses 16-bit or 24-bit PCM
      config.playback.format = ma_format_s16; // Default to 16-bit PCM for FLAC
    }
    else
    {
      // MP3 is typically decoded to 32-bit float PCM
      config.playback.format = ma_format_f32; // Default to 32-bit PCM for MP3
    }

    config.dataCallback = pcmDataCallback;

    // Initialize the audio playback device
    if (ma_device_init(nullptr, &config, &device) != MA_SUCCESS)
    {
      PLUGIN_LOG_ERROR(_AUDIO_BACKEND_TYPE_) << "Device init failed.";
      return false;
    }

    return true;
  }

  void play() override
  {
    isPlaying = true;
    if (ma_device_start(&device) != MA_SUCCESS)
    {
      PLUGIN_LOG_ERROR(_AUDIO_BACKEND_TYPE_) << "Failed to start device.";
      throw std::runtime_error("Device start failed");
    }

    // Play the audio data (wait while it's playing)
    while (isPlaying)
    {
      ma_sleep(10); // Sleep for a shorter time to improve responsiveness
    }
  }

  [[nodiscard]] auto name() const -> const char* override { return "MiniAudio Plugin Backend"; }

  ~MiniAudioBackend() override
  {
    PLUGIN_LOG_INFO(_AUDIO_BACKEND_TYPE_) << "Cleaning up MiniAudioBackend.";
    ma_device_uninit(&device);
  }
};

} // namespace libwavy::audio
