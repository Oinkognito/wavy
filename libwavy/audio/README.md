# Audio Backends

Similar to transport stream plugin [here](https://github.com/Oinkognito/Wavy/blob/main/libwavy/tsfetcher/plugin/README.md), for audio backends there is a plugin manager that can load custom backends without a hassle

This is pretty neat as:

1. I am not great at writing a great and efficient audio player/backend
2. Works very well for user's customizability of preferred audio player

> [!WARNING]
> 
> The way the plugins handle FLAC audio stream works ONLY for 32 bit signed (lower endian)
> 
> In the very upcoming future, I plan on modularising it for 16, 24(maybe) and 32 bit FLAC audio streams.
> 
> Most MP3 I have encountered so far all have FLTP so I am sticking with just that format for now.
> 

## Default Backend 

The audio backend by default is `miniaudio` with minimal implementation works decently well for mp3 and flac (16bit only) [here](https://github.com/Oinkognito/Wavy/blob/main/libwavy/audio/backends/miniaudio/entry.hpp)

> [!NOTE]
> 
> Technically, most Linux audio backends like **ALSA**, **PulseAudio**, etc.
> *should* work with this as `libwavy::ffmpeg::decoder` handles decoding audio
> to raw **PCM** data, which can be played back using the above backends.
> 
> It is important to note however, that I have **NOT** yet tried and tested this.
> 

In the future, there will be an in-detailed explanation of how to write a basic audio backend plugin that `libwavy::audio::plugin::WavyAudioBackend` plugin manager can read

For reference, take a look at [PulseAudio](https://github.com/Oinkognito/Wavy/blob/main/libwavy/audio/backends/pulseaudio/entry.hpp)

Alsa also works great 
