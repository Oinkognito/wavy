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

#include <alsa/asoundlib.h>
#include <alsa/pcm.h>
#include <libwavy/audio/interface.hpp>
#include <libwavy/utils/io/log/entry.hpp>
#include <vector>

using namespace libwavy::utils::pluginlog;

namespace libwavy::audio
{

constexpr const char* _AUDIO_BACKEND_NAME_ = "ALSA";

class AlsaAudioBackend : public IAudioBackend
{
private:
  snd_pcm_t*                 handle{nullptr};
  std::vector<unsigned char> audioData;
  bool                       isPlaying{false};
  snd_pcm_format_t           format{SND_PCM_FORMAT_FLOAT_LE}; // Default: float32 interleaved

public:
  auto initialize(const std::vector<unsigned char>& audioInput, bool isFlac,
                  int preferredSampleRate, int preferredChannels, int bitDepth = 16)
    -> bool override
  {
    audioData = audioInput;
    ::libwavy::utils::pluginlog::set_default_tag(_AUDIO_BACKEND_NAME_);

    if (isFlac)
    {

      format              = SND_PCM_FORMAT_S32_LE;
      preferredSampleRate = 44100;

      PLUGIN_LOG_INFO() << "FLAC PCM data found -> setting sample rate to " << preferredSampleRate;
    }

    const char* device = "default";
    int         err;

    if ((err = snd_pcm_open(&handle, device, SND_PCM_STREAM_PLAYBACK, 0)) < 0)
    {
      PLUGIN_LOG_ERROR() << "Failed to open ALSA device: " << snd_strerror(err);
      return false;
    }

    int rate     = (preferredSampleRate > 0) ? preferredSampleRate : 48000;
    int channels = (preferredChannels > 0) ? preferredChannels : 2;

    snd_pcm_hw_params_t* hw_params;
    snd_pcm_hw_params_alloca(&hw_params);
    snd_pcm_hw_params_any(handle, hw_params);
    snd_pcm_hw_params_set_access(handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(handle, hw_params, format);
    snd_pcm_hw_params_set_channels(handle, hw_params, channels);
    snd_pcm_hw_params_set_rate_near(handle, hw_params, (unsigned int*)&rate, nullptr);

    if ((err = snd_pcm_hw_params(handle, hw_params)) < 0)
    {
      PLUGIN_LOG_ERROR() << "Failed to set ALSA HW params: " << snd_strerror(err);
      return false;
    }

    PLUGIN_LOG_INFO() << "ALSA Backend initialized successfully.";
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

      snd_pcm_sframes_t frames = snd_pcm_writei(handle, audioData.data() + offset,
                                                toWrite / (sizeof(float) * 2)); // stereo float
      if (frames < 0)
      {
        frames = snd_pcm_recover(handle, frames, 0);
        if (frames < 0)
        {
          PLUGIN_LOG_ERROR() << "ALSA write failed: " << snd_strerror(frames);
          break;
        }
      }

      offset += toWrite;
    }

    snd_pcm_drain(handle);
    isPlaying = false;
  }

  [[nodiscard]] auto name() const -> const char* override { return "ALSA Plugin Backend"; }

  ~AlsaAudioBackend() override
  {
    PLUGIN_LOG_INFO() << "Cleaning up AlsaAudioBackend.";
    isPlaying = false;

    if (handle)
    {
      snd_pcm_close(handle);
      handle = nullptr;
    }
  }
};

} // namespace libwavy::audio
