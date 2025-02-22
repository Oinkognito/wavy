#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
#include <vector>

#define BUFFER_SIZE 4096
#define SAMPLE_RATE 44100
#define CHANNELS    2

class AudioPlayer
{
private:
  ma_device                  device;
  ma_decoder                 decoder;
  std::vector<unsigned char> audioMemory; // Hold the Audio data to keep it valid during playback
  bool                       isPlaying;

  static void dataCallback(ma_device* pDevice, void* pOutput, const void* pInput,
                           ma_uint32 frameCount)
  {
    auto* player = (AudioPlayer*)pDevice->pUserData;
    auto* output = (float*)pOutput;

    // Read directly from decoder
    ma_uint64 framesRead;
    ma_decoder_read_pcm_frames(&player->decoder, output, frameCount, &framesRead);

    // Fill remaining with silence if we reached the end
    if (framesRead < frameCount)
    {
      ma_silence_pcm_frames(output + (framesRead * CHANNELS), frameCount - framesRead,
                            ma_format_f32, CHANNELS);
      player->isPlaying = false;
    }
  }

public:
  // New constructor that initializes decoder from memory
  AudioPlayer(const std::vector<unsigned char>& audioInput)
      : audioMemory(audioInput), isPlaying(false)
  {
    ma_decoder_config decoderConfig = ma_decoder_config_init(ma_format_f32, CHANNELS, SAMPLE_RATE);

    if (ma_decoder_init_memory(audioMemory.data(), audioMemory.size(), &decoderConfig, &decoder) !=
        MA_SUCCESS)
    {
      throw std::runtime_error("Failed to initialize decoder from memory");
    }

    ma_device_config config  = ma_device_config_init(ma_device_type_playback);
    config.playback.format   = ma_format_f32;
    config.playback.channels = CHANNELS;
    config.sampleRate        = SAMPLE_RATE;
    config.dataCallback      = dataCallback;
    config.pUserData         = this;

    if (ma_device_init(nullptr, &config, &device) != MA_SUCCESS)
    {
      ma_decoder_uninit(&decoder);
      throw std::runtime_error("Failed to initialize audio device");
    }
  }

  ~AudioPlayer()
  {
    ma_device_uninit(&device);
    ma_decoder_uninit(&decoder);
  }

  void play()
  {
    if (ma_device_start(&device) != MA_SUCCESS)
    {
      throw std::runtime_error("Failed to start audio device");
    }
    isPlaying = true;

    // Wait while playing
    while (isPlaying)
    {
      ma_sleep(100);
    }

    ma_device_stop(&device);
  }
};

/*auto main() -> int*/
/*{*/
/*  // Read Audio data from standard input*/
/*  std::vector<unsigned char> audioData((std::istreambuf_iterator<char>(std::cin)),*/
/*                                       std::istreambuf_iterator<char>());*/
/*  if (audioData.empty())*/
/*  {*/
/*    std::cerr << "No Audio input received from STDIN" << std::endl;*/
/*    return 1;*/
/*  }*/
/**/
/*  try*/
/*  {*/
/*    AudioPlayer player(audioData);*/
/*    player.play();*/
/*  }*/
/*  catch (const std::exception& e)*/
/*  {*/
/*    std::cerr << "Error: " << e.what() << std::endl;*/
/*    return 1;*/
/*  }*/
/**/
/*  return 0;*/
/*}*/
