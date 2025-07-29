#pragma once
/********************************************************************************
 *                                Wavy Project                                  *
 *                         High-Fidelity Audio Streaming                        *
 *                                                                              *
 *  Copyright (c) 2025 Oinkognito                                               *
 *  All rights reserved.                                                        *
 *                                                                              *
 *  License:                                                                    *
 *  This software is licensed under the BSD-3-Clause License. You may use,      *
 *  modify, and distribute this software under the conditions stated in the     *
 *  LICENSE file provided in the project root.                                  *
 *                                                                              *
 *  Warranty Disclaimer:                                                        *
 *  This software is provided "AS IS", without any warranties or guarantees,    *
 *  either expressed or implied, including but not limited to fitness for a     *
 *  particular purpose.                                                         *
 *                                                                              *
 *  Contributions:                                                              *
 *  Contributions are welcome. By submitting code, you agree to license your    *
 *  contributions under the same BSD-3-Clause terms.                            *
 *                                                                              *
 *  See LICENSE file for full legal details.                                    *
 ********************************************************************************/

#include <condition_variable>
#include <libwavy/audio/interface.hpp>
#include <libwavy/common/state.hpp>
#include <libwavy/utils/io/pluginlog/entry.hpp>
#include <pulse/error.h>
#include <pulse/simple.h>
#include <termios.h>

using namespace libwavy::utils::pluginlog;

namespace libwavy::audio
{

constexpr AudioBackendPluginName _AUDIO_BACKEND_NAME_ = "PulseAudio";
using PulseError                                      = int;

class PulseAudioBackend : public IAudioBackend
{
private:
  pa_simple*            m_stream{nullptr};
  TotalDecodedAudioData m_data;
  std::atomic<bool>     m_isPlaying{false};
  pa_sample_spec        m_sampleSpec{};

public:
  auto initialize(const TotalDecodedAudioData& audioInput, bool isFlac, int preferredSampleRate,
                  int preferredChannels, int /*bitDepth*/ = 16) -> bool override
  {
    m_data = audioInput;

    ::libwavy::utils::pluginlog::set_default_tag(_AUDIO_BACKEND_NAME_);

    if (isFlac)
      preferredSampleRate = 44100;

    m_sampleSpec.format =
      isFlac ? PA_SAMPLE_S32LE
             : PA_SAMPLE_FLOAT32LE; // Supports float PCM (common for decoded FLAC and MP3)
    m_sampleSpec.rate     = (preferredSampleRate > 0) ? preferredSampleRate : 48000;
    m_sampleSpec.channels = (preferredChannels > 0) ? preferredChannels : 2;

    int error;
    m_stream = pa_simple_new(nullptr, "WavyClient-Pulseaudio", PA_STREAM_PLAYBACK, nullptr,
                             "playback", &m_sampleSpec, nullptr, nullptr, &error);

    if (!m_stream)
    {
      PLUGIN_LOG_ERROR() << "Failed to initialize PulseAudio: " << pa_strerror(error);
      return false;
    }

    PLUGIN_LOG_INFO() << "PulseAudio Backend initialized successfully.";
    return true;
  }

  void play() override
  {
    if (!m_stream || m_data.empty())
    {
      PLUGIN_LOG_ERROR() << "No m_stream or audio data to play.";
      return;
    }

    std::atomic<AudioOffset> offset{0};
    std::atomic<bool>        stopFlag{false};
    std::atomic<bool>        pauseFlag{false};
    std::mutex               pauseMutex;
    std::condition_variable  pauseCond;

    m_isPlaying = true;

    // Playback thread
    std::thread playbackThread(
      [&]()
      {
        const AudioChunk chunkSize = 4096;

        while (!stopFlag && offset < m_data.size())
        {
          {
            std::unique_lock lock(pauseMutex);
            pauseCond.wait(lock, [&]() { return !pauseFlag || stopFlag; });
          }

          if (stopFlag)
            break;

          AudioChunk remaining = m_data.size() - offset;
          AudioChunk toWrite   = std::min(remaining, chunkSize);
          PulseError error;

          if (pa_simple_write(m_stream, m_data.data() + offset, toWrite, &error) < 0)
          {
            PLUGIN_LOG_ERROR() << "PulseAudio write failed: " << pa_strerror(error);
            break;
          }

          offset += toWrite;
        }

        pa_simple_drain(m_stream, nullptr);
        m_isPlaying = false;
      });

    // Utility: getch (non-blocking input)
    auto getch = []() -> char
    {
      char           buf = 0;
      struct termios old = {};
      tcgetattr(STDIN_FILENO, &old);
      old.c_lflag &= ~(ICANON | ECHO);
      tcsetattr(STDIN_FILENO, TCSANOW, &old);
      read(STDIN_FILENO, &buf, 1);
      old.c_lflag |= (ICANON | ECHO);
      tcsetattr(STDIN_FILENO, TCSANOW, &old);
      return buf;
    };

    std::cout << "----- Wavy Audio CLI -----\n";
    std::cout << "[p] Play/Pause | [s] Seek | [q] Quit\n";

    while (!stopFlag)
    {
      char c = getch();
      if (c == 'p')
      {
        pauseFlag = !pauseFlag;
        if (!pauseFlag)
          pauseCond.notify_one();
      }
      else if (c == 's')
      {
        std::cout << "\nSeek to (sec): ";
        float sec;
        std::cin >> sec;

        auto newOffset = static_cast<AudioOffset>(sec * m_sampleSpec.rate);
        if (newOffset < m_data.size())
        {
          std::unique_lock lock(pauseMutex);
          offset = newOffset;
          pa_simple_flush(m_stream, nullptr);
        }
      }
      else if (c == 'q')
      {
        stopFlag = true;
        pauseCond.notify_one();
        break;
      }

      if (m_sampleSpec.rate > 0)
      {
        std::cout << "\rProgress: " << offset / m_sampleSpec.rate << "s / "
                  << m_data.size() / m_sampleSpec.rate << "s" << std::flush;
      }
      else
      {
        std::cout << "\rProgress: (rate undefined)" << std::flush;
      }

      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    if (playbackThread.joinable())
      playbackThread.join();
  }

  [[nodiscard]] auto name() const -> AudioBackendPluginName override
  {
    return "PulseAudio Plugin Backend";
  }

  ~PulseAudioBackend() override
  {
    PLUGIN_LOG_INFO() << "Cleaning up PulseAudioBackend.";
    m_isPlaying = false;

    if (m_stream)
    {
      pa_simple_free(m_stream);
      m_stream = nullptr;
    }
  }
};

} // namespace libwavy::audio
