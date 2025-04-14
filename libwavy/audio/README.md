# Audio Backends

Similar to transport stream plugin [here](https://github.com/Oinkognito/Wavy/blob/main/libwavy/tsfetcher/plugin/README.md), for audio backends there is a plugin manager that can load custom backends without a hassle

This is pretty neat as:

1. I am not great at writing a great and efficient audio player/backend
2. Works very well for user's customizability of preferred audio player

## Default Backend 

The audio backend by default is `miniaudio` with minimal implementation works decently well for mp3 and flac (16bit only) [here](https://github.com/Oinkognito/Wavy/blob/main/libwavy/audio/backends/miniaudio/entry.hpp)

In the future, there will be an in-detailed explanation of how to write a basic audio backend plugin that `libwavy::audio::plugin::WavyAudioBackend` plugin manager can read
