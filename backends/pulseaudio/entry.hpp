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

#include <cstring>
#include <libwavy/audio/interface.hpp>
#include <libwavy/utils/io/log/entry.hpp>
#include <pulse/error.h>
#include <pulse/simple.h>
#include <vector>

using namespace libwavy::utils::pluginlog;

namespace libwavy::audio
{

constexpr const char* _AUDIO_BACKEND_NAME_ = "PulseAudio";

class PulseAudioBackend : public IAudioBackend
{
private:
  pa_simple*                 stream{nullptr};
  std::vector<unsigned char> audioData;
  bool                       isPlaying{false};

public:
  auto initialize(const std::vector<unsigned char>& audioInput, bool isFlac,
                  int preferredSampleRate, int preferredChannels, int /*bitDepth*/ = 16)
    -> bool override
  {
    audioData = audioInput;

    ::libwavy::utils::pluginlog::set_default_tag(_AUDIO_BACKEND_NAME_);

    if (isFlac)
      preferredSampleRate = 44100;

    pa_sample_spec sampleSpec;
    sampleSpec.format =
      isFlac ? PA_SAMPLE_S32LE
             : PA_SAMPLE_FLOAT32LE; // Supports float PCM (common for decoded FLAC and MP3)
    sampleSpec.rate     = (preferredSampleRate > 0) ? preferredSampleRate : 48000;
    sampleSpec.channels = (preferredChannels > 0) ? preferredChannels : 2;

    int error;
    stream = pa_simple_new(nullptr, "Wavy", PA_STREAM_PLAYBACK, nullptr, "playback", &sampleSpec,
                           nullptr, nullptr, &error);

    if (!stream)
    {
      PLUGIN_LOG_ERROR() << "Failed to initialize PulseAudio: " << pa_strerror(error);
      return false;
    }

    PLUGIN_LOG_INFO() << "PulseAudio Backend initialized successfully.";
    return true;
  }

  void play() override
  {
    isPlaying = true;

    size_t       offset    = 0;
    const size_t chunkSize = 4096;

    while (isPlaying && offset < audioData.size())
    {
      size_t remaining = audioData.size() - offset;
      size_t toWrite   = (remaining < chunkSize) ? remaining : chunkSize;

      int error;
      if (pa_simple_write(stream, audioData.data() + offset, toWrite, &error) < 0)
      {
        PLUGIN_LOG_ERROR() << "PulseAudio write failed: " << pa_strerror(error);
        break;
      }

      offset += toWrite;
    }

    pa_simple_drain(stream, nullptr);
    isPlaying = false;
  }

  [[nodiscard]] auto name() const -> const char* override { return "PulseAudio Plugin Backend"; }

  ~PulseAudioBackend() override
  {
    PLUGIN_LOG_INFO() << "Cleaning up PulseAudioBackend.";
    isPlaying = false;

    if (stream)
    {
      pa_simple_free(stream);
      stream = nullptr;
    }
  }
};

} // namespace libwavy::audio
