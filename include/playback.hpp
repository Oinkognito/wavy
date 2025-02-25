#define MINIAUDIO_IMPLEMENTATION
#include "logger.hpp"
#include "miniaudio.h"
#include <iomanip> // for std::fixed and std::setprecision
#include <stdexcept>
#include <vector>

#define SAMPLE_RATE 44100
#define CHANNELS    2

class AudioPlayer
{
private:
  ma_engine                  engine;
  ma_decoder                 decoder;
  std::vector<unsigned char> audioMemory;
  bool                       isPlaying;
  ma_device                  device;

  static void dataCallback(ma_device* pDevice, void* pOutput, const void* pInput,
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

public:
  AudioPlayer(const std::vector<unsigned char>& audioInput)
      : audioMemory(audioInput), isPlaying(false)
  {
    LOG_INFO << "Initializing AudioPlayer with " << audioMemory.size() << " bytes of audio data.";

    if (ma_engine_init(nullptr, &engine) != MA_SUCCESS)
    {
      LOG_ERROR << "Failed to initialize audio engine.";
      throw std::runtime_error("Failed to initialize audio engine");
    }

    ma_decoder_config decoderConfig = ma_decoder_config_init(ma_format_f32, CHANNELS, SAMPLE_RATE);
    if (ma_decoder_init_memory(audioMemory.data(), audioMemory.size(), &decoderConfig, &decoder) !=
        MA_SUCCESS)
    {
      LOG_ERROR << "Failed to initialize decoder from memory.";
      ma_engine_uninit(&engine);
      throw std::runtime_error("Failed to initialize decoder from memory");
    }

    LOG_INFO << "Decoder initialized: "
             << "Format: " << decoder.outputFormat << ", Channels: " << decoder.outputChannels
             << ", Sample Rate: " << decoder.outputSampleRate;

    ma_uint64 totalFrames;
    if (ma_decoder_get_length_in_pcm_frames(&decoder, &totalFrames) == MA_SUCCESS)
    {
      double duration = static_cast<double>(totalFrames) / decoder.outputSampleRate;
      LOG_INFO << "Audio duration: " << std::fixed << std::setprecision(2) << duration
               << " seconds";
      LOG_INFO << "Total frames: " << totalFrames;
    }
    else
    {
      LOG_WARNING << "Could not determine audio duration.";
    }

    ma_device_config config  = ma_device_config_init(ma_device_type_playback);
    config.playback.format   = ma_format_f32;
    config.playback.channels = CHANNELS;
    config.sampleRate        = SAMPLE_RATE;
    config.dataCallback      = dataCallback;
    config.pUserData         = this;

    if (ma_device_init(nullptr, &config, &device) != MA_SUCCESS)
    {
      LOG_ERROR << "Failed to initialize audio device.";
      ma_decoder_uninit(&decoder);
      ma_engine_uninit(&engine);
      throw std::runtime_error("Failed to initialize audio device");
    }

    LOG_INFO << "Audio device initialized: "
             << "Format: " << config.playback.format << ", Channels: " << config.playback.channels
             << ", Sample Rate: " << config.sampleRate;

    LOG_INFO << "Audio engine and decoder initialized successfully.";
  }

  ~AudioPlayer()
  {
    LOG_INFO << "Shutting down AudioPlayer.";
    ma_device_uninit(&device);
    ma_decoder_uninit(&decoder);
    ma_engine_uninit(&engine);
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
