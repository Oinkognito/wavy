# Writing Audio Backends for Wavy 

> [!NOTE]
> 
> This README can become outdated quite fast due to the 
> heavy dev nature of Wavy at the moment. 
> 
> Despite that, it is **highly** advised to go through this atleast once
> to understand the goal and baseline of how the audio backend functions.
> 

Audio backends are designed to be as **PLUGINS** for Wavy for user customizability and convenience. It allows for lesser required dependencies all-the-while having support for popular existing Linux backends like PulseAudio, ALSA, etc.

In the future, these plugins can (and should) become a fully-fledged music daemon that can provide audio controls and information with a neat user interface (GUI/TUI/CLI)

Before understanding how to write a basic audio backend for Wavy, let us first understand the interface to be implemented:

## Interface for Audio Backend 

The `IAudioBackend` interface defines a standard contract for implementing audio playback backends within the `libwavy::audio` namespace. It allows for polymorphic handling of different audio systems (e.g., PulseAudio, ALSA, etc.) by abstracting away the underlying implementation details.

### Purpose

This interface enables the `libwavy` system to support multiple audio output backends through a common set of methods. Each backend plugin must inherit from `IAudioBackend` and provide concrete implementations for the declared virtual methods.

### Interface Definition

```cpp
class IAudioBackend
{
public:
  virtual ~IAudioBackend() = default;

  virtual auto initialize(const std::vector<unsigned char>& audioInput, bool isFlac,
                          int preferredSampleRate = 0, int preferredChannels = 0, int bitDepth = 16)
    -> bool = 0;

  virtual void play() = 0;

  [[nodiscard]] virtual auto name() const -> const char* = 0;
};
```

Quite a simple interface with only three functions to implement. 

A question that can be posed here is why so few methods of implementation? Because the audio backend plugin should ideally take care of the rest (in my opinion atleast). 

Primary reason is for a better and more unified way of creating plugins without a hassle.

Also, the job of **WavyClient** is essentially to encompass the jobs of `fetching`, `decoding` and `playback` in an environment. Since the plugin comes under Wavy Client *technically*, I am more than happy to hand this over to a plugin and work on the environment separately that is NOT part of `libwavy`.

The goal for `libwavy` is a simple and super handy abstration of existing libraries and concepts. Not a complicated library with lots of methods that tries to do multiple things on a large scale. 

### Methods to Implement

1. **`initialize(...) -> bool`**
   - Accepts raw audio data and optional playback parameters such as sample rate, channel count, and bit depth.
   - Returns `true` if the backend is initialized successfully.
   - This is where audio stream configuration and backend-specific setup (e.g., opening audio device) occurs.

2. **`play()`**
   - Begins audio playback using the previously supplied audio data.
   - Implementations should handle audio streaming and possible errors internally.

3. **`name() -> const char*`**
   - Returns a human-readable name for the backend.
   - Useful for logging or debugging purposes to identify the active backend.

### Use Case

This interface supports plugin-based extensibility, allowing new backends to be added with minimal changes to the core system. It enforces a consistent API for all audio playback implementations.

## Plugin for Audio Backend

The `WavyAudioBackend` class provides a static method for dynamically loading audio backend plugins at runtime. It is part of the `libwavy::audio::plugin` namespace and plays a crucial role in the plugin-based architecture of the `libwavy` audio system.

### Purpose

This class is responsible for:

- Loading a shared object (`.so`) file from disk using `dlopen`
- Resolving required function symbols (`create_audio_backend` and `get_plugin_metadata`)
- Creating an instance of the backend via the resolved factory function
- Wrapping the backend instance in a `std::unique_ptr` with a custom deleter that also unloads the plugin

### Key Features

- **Runtime Plugin Discovery**  
  The `load()` method takes a path to a shared object file and attempts to load it using `dlopen`.

- **Symbol Resolution**  
  It expects the plugin to export two symbols:
  - `create_audio_backend`: Factory function returning a pointer to an `IAudioBackend`
  - `get_plugin_metadata`: Function returning a descriptive string for logging or metadata purposes

- **Error Handling**  
  Provides detailed error logs and throws `std::runtime_error` if plugin loading or symbol resolution fails.

- **Safe Resource Management**  
  The returned `std::unique_ptr` ensures proper destruction of the backend object and cleanup of the loaded shared library using a custom deleter lambda.

### Code Overview

```cpp
static auto load(const std::string& plugin_path)
  -> std::unique_ptr<IAudioBackend, std::function<void(IAudioBackend*)>>;
```

#### How It Works

1. **`dlopen(plugin_path.c_str(), RTLD_LAZY)`**
   - Opens the shared library.
   - If it fails, an error is logged and an exception is thrown.

2. **`dlsym(..., "create_audio_backend")`**
   - Resolves the backend creation function.
   - `IAudioBackend* create_audio_backend()` must be defined in the plugin.

3. **`dlsym(..., "get_plugin_metadata")`**
   - Resolves a function that returns plugin metadata for logging.

4. **Constructs and returns a managed pointer:**
   ```cpp
   return {backend_ptr, [handle](IAudioBackend* ptr) {
     delete ptr;
     dlclose(handle);
   }};
   ```

### Requirements for Audio Plugins

To work with this loader, a plugin must:
- Be compiled as a shared object (.so)
- Export a `create_audio_backend()` function that returns a new `IAudioBackend*`
- Export a `get_plugin_metadata()` function that returns a `const char*` description

### Example Plugin Symbol Definition

```cpp
extern "C" IAudioBackend* create_audio_backend() {
  return new MyCustomAudioBackend();
}

extern "C" const char* get_plugin_metadata() {
  return "MyCustomAudioBackend v1.0";
}
```

Ideally, you should be having `MyCustomAudioBackend` class under `libwavy::audio` namespace for uniformity.

## Writing your Audio Backend 

Finally we can start writing a basic audio backend for Wavy. It is quite simple if you have been following along.

> [!NOTE]
> 
> It is **HIGHLY** advised to also take a look at existing [audio backends](https://github.com/Oinkognito/Wavy/blob/main/backends/)
> 
> It has currently, the basic plugin implementations of `PulseAudio` and `ALSA` with simple playback for MP3 and FLAC (32 bit only)
>

### Key points before kicking off 

1. **ALWAYS WRITE LOGS USING `PLUGIN_LOG_*`**:

`PLUGIN_LOG_*` is part of `libwavy/utils/io/log` under no particular namespace. To avoid heavy compilation times using **Boost.Log**, this ia simple logging system for plugins with basic colors and neatly generated logs.

2. **INCLUDE `libwavy/audio/interface.hpp`**:

This should be quite obvious but without including this header, your plugin is useless.

3. **EVERY REQUIRED METHOD SHOULD `override`**:

The required methods `initialize`, `play` and `name` should override the existing methods in the interface (self-explanatory).

So as an example,

```cpp
class TestAudioBackend : public IAudioBackend
{
public:
    // NO OVERRIDE [X] ==> **WRONG**
    void play() 
    {
        // ...
    }

    // OVERRIDE [!] ==> **CORRECT**
    void play() override
    {
        // ...
    }
};
```

3. **WRITING CMakeLists.txt FOR PLUGIN**:

There is a template for writing CMakeLists.txt for audio backend plugins: 

```cmake 
set(AUD_BACKEND_TITLE "Test Backend")

# !!! KEEP THIS UNCHANGED !!!
set(AUD_BACKEND_NAME wavy_audio_backend_${AUD_BACKEND_TITLE}_plugin)
set(AUD_BACKEND_SRC src/entry.cpp)

# !!! KEEP THIS UNCHANGED !!!
message(STATUS "  >>> Building AUDIO BACKEND PLUGIN: ${AUD_BACKEND_TITLE}")
message(STATUS "  >>> Target: ${AUD_BACKEND_NAME}")
message(STATUS "  >>> Output Directory: ${WAVY_AUDIO_BACKEND_PLUGIN_OUTPUT_PATH}\n")

# Find required audio backend using pkg-config (CHANGE AS YOU PLEASE)
find_package(PkgConfig REQUIRED)
pkg_check_modules(PULSEAUDIO REQUIRED libpulse-simple)
pkg_check_modules(FFMPEG REQUIRED libavutil libavformat libavcodec libswresample)

# !!! KEEP THIS UNCHANGED !!!
# IMPORTANT: MAKE YOUR PLUGIN A SHARED OBJECT FILE (.SO)
add_library(${AUD_BACKEND_NAME} SHARED ${AUD_BACKEND_SRC})

# INCLUDE REQUIRED DIRECTORIES YOUR DESIRED AUDIO BACKEND TO PLUGIN
target_include_directories(${AUD_BACKEND_NAME} PRIVATE
  ${CMAKE_SOURCE_DIR}
  ${ZSTD_INCLUDE_DIRS}
  ${PULSEAUDIO_INCLUDE_DIRS}
  ${FFMPEG_INCLUDE_DIRS}
)

# LINK REQUIRED LIBRARIES YOUR DESIRED AUDIO BACKEND TO PLUGIN
target_link_libraries(${AUD_BACKEND_NAME} PRIVATE
  ${ZSTD_LIBRARIES}
  ${PULSEAUDIO_LIBRARIES}
  ${FFMPEG_LIBRARIES}
)

# !!! KEEP THIS UNCHANGED !!!
set_target_properties(${AUD_BACKEND_NAME} PROPERTIES
  LIBRARY_OUTPUT_DIRECTORY ${WAVY_AUDIO_BACKEND_PLUGIN_OUTPUT_PATH}
)
```

This is located [here](https://github.com/Oinkognito/Wavy/blob/main/assets/templates/plugin/audio-backend/CMakeLists.txt) for more details.

### Writing Audio Backend Class 

In this basic tutorial, I will use the example of a basic `PulseAudio Simple API` integration with Wavy. You can read the code for yourself [here](https://github.com/Oinkognito/Wavy/blob/main/backends/pulseaudio/entry.hpp)

Firstly you need to write a class that **SHOULD** have the following entry methods:

1. `initialize`
2. `play`
3. `name`

You are obviously NOT limited to just these methods, but it is important to understand that ONLY these methods will be called in `Wavy Client`.

#### What should `initialize` do?

You can do the following here:

1. Pass over `audioInput` to a member variable to your class 
2. Initialize the player by passing over `preferredSampleRate`, `preferredChannels`, etc. to your audio backend
3. Also ensure that you handle the **Sample Format** (with respect to `FLAC` or `MP3` for now) for proper audio playback 
4. Misc tasks

With respect to the `PulseAudio` plugin, it looks like so:

```cpp 
  auto initialize(const std::vector<unsigned char>& audioInput, bool isFlac,
                  int preferredSampleRate, int preferredChannels, int /*bitDepth*/ = 16)
    -> bool override
  {
    audioData = audioInput;

    if (isFlac) preferredSampleRate = 44100;

    pa_sample_spec sampleSpec;
    sampleSpec.format = isFlac ? PA_SAMPLE_S32LE : PA_SAMPLE_FLOAT32LE;
    sampleSpec.rate   = (preferredSampleRate > 0) ? preferredSampleRate : 48000;
    sampleSpec.channels = (preferredChannels > 0) ? preferredChannels : 2;

    int error;
    stream = pa_simple_new(nullptr, "Wavy", PA_STREAM_PLAYBACK, nullptr, "playback", &sampleSpec,
                           nullptr, nullptr, &error);

    if (!stream)
    {
      PLUGIN_LOG_ERROR(_AUDIO_BACKEND_NAME_)
        << "Failed to initialize PulseAudio: " << pa_strerror(error);
      return false;
    }

    PLUGIN_LOG_INFO(_AUDIO_BACKEND_NAME_) << "PulseAudio Backend initialized successfully.";
    return true;
  }
```

It is important to understand that this is **VERY RUDIMENTARY** way of handling this. This is **NOT** the ideal standard of writing the best plugin, but this is a good enough entry point to showcase how it *can* be written.

The above example should be clear enough regarding what is happening. It simply involves required steps for initialization of audio backend with respect to decoded PCM in `audioInput`.

#### What should `play` do? 

Simple. Play the audio directly or in a dedicated environment (GUI/TUI/CLI) with everything else upto the user on playback customizability and information display.

This is where your creative freedom should really shine. The main thing that `Wavy Client` expects from this is just playback of decoded stream. How you choose to do it, is TOTALLY upto you.

Here is how our basic `PulseAudio` backend is designed:

```cpp 
// audioData contains the decoded PCM data
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
        PLUGIN_LOG_ERROR(_AUDIO_BACKEND_NAME_) << "PulseAudio write failed: " << pa_strerror(error);
        break;
      }

      offset += toWrite;
    }

    pa_simple_drain(stream, nullptr);
    isPlaying = false;
}
```

It is just simple playback in this case with some basic error handling. You can refer to `PulseAudio Simple` API to understand about `draining` and `pa_simple_write`.

This method is totally upto the user for customization and implementation, there are no limitations unless it somehow breaks `Wavy Client` or violates the Wavy architectural schematic.

#### What does `name` do?

Just pass the name of the plugin over to name in a string like so:

```cpp 
[[nodiscard]] auto name() const -> const char* override { return "PulseAudio Plugin Backend"; }
```

Nothing more to it.

## Writing your entry.cpp 

This is the final step of your plugin, the one that really matters.

`entry.cpp` is the entry of your plugin, where the symbols `create_audio_backend` and `get_plugin_metadata` are read. This is VERY simple just look at this example:

```cpp 
#include "../entry.hpp"

extern "C"
{
  auto create_audio_backend() -> libwavy::audio::IAudioBackend*
  {
    return new libwavy::audio::PulseAudioBackend();
  }

  auto get_plugin_metadata() -> const char*
  {
    return "PulseAudio (PA) Plugin Backend v1.0 (supports MP3/FLAC)";
  }
}
```

You just pass over your class in `create_audio_backend` and the name of your plugin in `get_plugin_metadata` and thats it!

Here is the ideal source tree of how your plugin should look like:

```bash 
.
├── CMakeLists.txt # Compiling logic for your plugin that is referenced as a subdirectory in Wavy
├── plugin.hpp      # Your plugin logic that implements IAudioBackend here
├── plugin.toml    # [Optional]: Just basic plugin info
└── src
    └── entry.cpp  # Implementation of two symbols read at runtime
```

## Compiling it with the main Wavy Build System 

It is **HIGHLY** recommended to compile your plugin as part of the `Wavy's CMake build system` for more uniformity and ease of accessing your plugin.

Just head over to Wavy's root [CMakeLists.txt](https://github.com/Oinkognito/Wavy/blob/main/CMakeLists.txt) **line number 140**:

```cmake 
if (DEFINED BUILD_AUDIO_BACKEND_PLUGINS AND BUILD_AUDIO_BACKEND_PLUGINS)
########################### ADD AUDIO BACKEND PLUGINS HERE #########################################
set(WAVY_AUDIO_BACKEND_PLUGIN_OUTPUT_PATH ${CMAKE_BINARY_DIR}/plugins/audio)
set(WAVY_AUDIO_BACKEND_PLUGIN_PATH ${CMAKE_CURRENT_SOURCE_DIR}/backends)
message(NOTICE "\n${BLUE}${BOLD}${UNDERLINE} *** COMPILING AUDIO BACKEND PLUGINS HERE *** ...${RESET}")
message(NOTICE "\n${GREEN}${BOLD}==================[ WAVY AUDIO BACKEND PLUGIN BUILD ]===================${RESET}")
add_subdirectory(${WAVY_AUDIO_BACKEND_PLUGIN_PATH}/pulseaudio)
# add_subdirectory(${WAVY_AUDIO_BACKEND_PLUGIN_PATH}/alsa) # {TO TRY ALSA OUT JUST UNCOMMENT THIS LINE!}
# Add more here ...
message(NOTICE "${GREEN}${BOLD}=========================================================================${RESET}\n")
########################### END AUDIO BACKEND PLUGINS HERE #########################################
else()
  message(STATUS "${YELLOW}${BOLD}${UNDERLINE}Skipping AUDIO BACKEND PLUGIN build routine...${RESET}")
endif()
```

It is key to note that it is *expected* of you to write your plugin in `backends/` directory of Wavy's root source dir. You *can* try to write it elsewhere and try to add it here and it *should* not break, but that is a problem you are going to have to resolve on your own. For the sake of simplicity, just add it in `backends` and follow the template `CMakeLists.txt` for the plugin and you are good to go.

You can simply add your plugin here like so:

```cmake 
# existing cmake...
add_subdirectory(${WAVY_AUDIO_BACKEND_PLUGIN_PATH}/path-to-your-plugin) # if you follow our standards of writing plugin in backends

add_subdirectory(your-abs-dir-to-plugin/) # absolute directory of where your plugin is located with it's CMakeLists visible
```

Then recompile with `-DBUILD_AUDIO_BACKEND_PLUGINS=ON` flag when running `make` like so:

```bash 
make rebuild "-DBUILD_AUDIO_BACKEND_PLUGINS=ON" # can concatenate this with your existing flags
```

That's it! Your plugin should now compile to a shared object file and should be in `build/plugins/audio` (with respect to Wavy's source tree dir). 

### Using your plugin

Just pass over your plugin's shared object name to `Wavy Client` like so:

```bash 
./build/wavy_client --ipAddr=127.0.0.1 --serverIP=127.0.0.1 --bitrate-stream=128 --fetchMode=aggr --index=1 --playFlac=false --audioBackendLibPath=<your-plugin-name>
```

Check out your plugin `CMakeLists.txt`'s definition of `AUD_BACKEND_NAME` for more information on its name or just check out the shared object file yourself.

## Footnotes 

This is still a **MASSIVE** Work-In-Progress (WIP). Stay on the lookout for more changes as I attempt to integrate a global structure that holds key information for the plugins to understand the key **audio detailing schema** like `sample rate`, `sample format`, `channels`, and so on, that are crucial information to be passed on to the audio backend for proper playback.

In the future hopefully, this becomes modular enough to support *as many** formats as possible (in regards to Wavy anyway)

Expect breaking changes as always, but this should be a great starting point for writing a basic audio backend plugin for Wavy!

## Conclusion

That concludes this basic tutorial, hopefully everything here seems logical and easy to follow, if there are any inconsistencies or unknown errors you face along the way, feel free to post an issue [here](https://github.com/Oinkognito/Wavy/issues)!
