#define MINIAUDIO_IMPLEMENTATION
#include "../include/miniaudio.h"
#include <boost/interprocess/mapped_region.hpp>
#include <boost/interprocess/shared_memory_object.hpp>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>

#define BUFFER_SIZE 4096
#define SAMPLE_RATE 44100
#define CHANNELS    2

void updatePlaybackTime(double currentTime)
{
  using namespace boost::interprocess;
  shared_memory_object shm_obj(open_or_create, "WavyPlaybackTime", read_write);
  shm_obj.truncate(sizeof(double));
  mapped_region region(shm_obj, read_write);

  double* playback_time_ptr = static_cast<double*>(region.get_address());
  *playback_time_ptr        = currentTime;
}

class AudioPlayer
{
private:
  ma_device  device;
  ma_decoder decoder;
  bool       isPlaying;

  static void dataCallback(ma_device* pDevice, void* pOutput, const void* /*pInput*/,
                           ma_uint32 frameCount)
  {
    AudioPlayer* player = reinterpret_cast<AudioPlayer*>(pDevice->pUserData);
    float*       output = reinterpret_cast<float*>(pOutput);

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
  AudioPlayer(const char* audioSource) : isPlaying(false)
  {
    ma_decoder_config decoderConfig = ma_decoder_config_init(ma_format_f32, CHANNELS, SAMPLE_RATE);

    if (ma_decoder_init_file(audioSource, &decoderConfig, &decoder) != MA_SUCCESS)
    {
      throw std::runtime_error("Failed to initialize decoder from source");
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

    auto start = std::chrono::high_resolution_clock::now();

    while (isPlaying)
    {
      auto   now     = std::chrono::high_resolution_clock::now();
      double elapsed = std::chrono::duration<double>(now - start).count();
      updatePlaybackTime(elapsed);
      ma_sleep(100);
    }

    ma_device_stop(&device);
  }
};

int main()
{
  try
  {
    AudioPlayer player("/dev/stdin");
    player.play();
  }
  catch (const std::exception& e)
  {
    std::cerr << "Error: " << e.what() << std::endl;
    boost::interprocess::shared_memory_object::remove("WavyPlaybackTime");
    return 1;
  }

  // Remove the shared memory object after playback ends.
  boost::interprocess::shared_memory_object::remove("WavyPlaybackTime");
  return 0;
}
