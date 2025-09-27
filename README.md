# **Wavy**

A **local networking solution** for **audio streaming and sharing**, supporting **lossless and lossy audio formats** through a unified platform, written in C++20.

> [!IMPORTANT]
>
> Wavy is still under heavy development (**WIP**)
>
> Expect a **LOT** of breaking changes with each commit.
>

## **Table of Contents**
- [**Introduction**](#introduction)
- [**Dependencies**](#dependencies)
- [**Building**](#building)
- [**Architecture**](#architecture)
- [**Security**](#security)
- [**API References**](#api-references)
- [**Dry Run**](#dry-run)
- [**Examples**](#examples)
- [**Server**](#server)
- [**Usage**](#usage)
- [**Documentation**](#documentation)
- [**License**](#license)

## **Introduction**
Wavy is a **lightweight** and **efficient** solution for audio streaming within a local network. It is designed to **encode, decode, and dispatch** audio streams seamlessly while ensuring **secure** data transfer via **SSL/TLS encryption**.

> [!WARNING]
>
> The Wavy Project currently is only supported for *NIX Operating Systems (on x86-64 architecture).
>
>
> Currently supported and tested distributions:
>
> 1. Arch Linux
> 2. Ubuntu (Debian)
> 3. Fedora
>
> In the future, perhaps Windows / MacOS will be supported.

It supports:
- **Lossless formats** (FLAC, ALAC, WAV)
- **Lossy formats** (MP3, AAC, Opus, Vorbis)
- **Adaptive bitrate streaming** (Soon) using **HLS (HTTP Live Streaming)**.
- **Metadata extraction** and **TOML-based** configuration.
- **Transport stream decoding** via **FFmpeg** for real-time audio playback.

## **Dependencies**
To build and run **Wavy**, install the following dependencies:

| Dependency  | Purpose |
|-------------|---------|
| **FFmpeg**  | Audio encoding, decoding, and streaming |
| **Base-devel** | Includes `g++` / `clang++` for C++ compilation |
| **OpenSSL** | Secure communication using SSL/TLS |
| **Boost C++** | Asynchronous networking & utility functions |
| **Intel oneTBB** | Parallelism for Wavy operations |
| **libzstd** | Lossless compression (Zstandard) |
| **CMake & Make** | Build system tools |
| **Pkg-Config** | Build system tools helper |
| **Libarchive** | Handling `.tar`, `.gz`, `.zst` compressed files |
| **libmp3lame** | MP3 encoding support |
| **libbacktrace** | For verbose and clean backtraces |

### **Optional Dependencies**

| Dependency  | Purpose |
|-------------|---------|
| **FLAC & FLAC++** | FLAC decoding and encoding for lossless streaming |
| **Qt6** (_Core, Widgets_) | Required for GUI development. |
| **Mold** (modern linker) | For faster linker times. |
| **Ninja** (Build system) | A smaller and lightweight build system. |

Here are the dependencies installation command for some common distributions:

> [!NOTE]
>
> It is important that you need to download the **DEVELOPMENT** version
> of these packages for `pkg-config` to work!
>
> If you are on `Arch Linux`, then ignore this.
>
> So let us say you have `libboost` downloaded in your `Ubuntu` system.
> `pkg-config` will **NOT** be able to find and link `libboost`!
>
> Hence, you would need to install `libboost-dev` and replace it with `libboost`.
>

To install these packages to your distro, it is recommended you go check out [ci](https://github.com/Oinkognito/wavy/blob/main/ci) that has the packages listed for each package manager.

### **Arch Linux**

> [!CAUTION]
>
> Run these from the root directory of Wavy!
>

```bash
xargs -a ci/pacman-packages.txt sudo pacman -S --needed
```

### **Ubuntu / Debian**

```bash
sudo apt update && xargs -a ci/apt-packages.txt sudo apt install -y
```

### **Fedora**

```bash
xargs -a ci/rpm-packages.txt sudo dnf install -y
```

## **Building**

> [!NOTE]
>
> Ensure you clone this repository **recursively**!
>
> ```bash
> git clone --recursive https://github.com/Oinkognito/wavy.git
> ```
>

Wavy has a lot of targets that can be built with a lot of customizability.

In general if you want things to just work:

```bash
make all # assuming you have setup all the required deps
```

If you want to configure dependencies and build the project: (For **Ubuntu**, **Fedora** and **Arch**)

```bash
./build.sh
# When a prompt comes up for which target to build just hit enter or type all
```

For more information on targets and in depth building details check out [BUILD.md](https://github.com/Oinkognito/wavy/blob/main/BUILD.md)

> [!IMPORTANT]
>
> If you want to contribute to Wavy and want compile times
> for each binary to be faster, here are a few steps that are recommended:
>
> 1. Use Ninja with the existing build system:
>
> ```bash
> make server EXTRA_CMAKE_FLAGS="-DBUILD_NINJA=ON" # add the BUILD_NINJA flag when making a target
> ```
>
> This should compile with parallelism and should give faster builds.
>
> 2. Use Mold as the linker:
>
> ```bash
> make all EXTRA_CMAKE_FLAGS="-DUSE_MOLD=ON" # add USE_MOLD flag when making a target
> ```
>
> This will try to use [mold](https://github.com/rui314/mold) as the linker for the project.
> You can try different flags but it is recommended that you do not. It is not necessary.
>
> Mold is significantly smarter and faster than GNU's `ld` and CLANGs `lld` and this should help in
> faster build and linking times for the project.
>


## **Architecture**
The **Wavy** system consists of the following components:

- **Encoder:** Converts audio files into **HLS (HTTP Live Streaming) format**.
- **Decoder:** Parses transport streams for playback.
- **Dispatcher:** Manages transport stream distribution.
- **Server:** Handles **secure** file uploads, downloads, and client session management.

**System Overview:**
<img src="assets/wavy-arch.jpeg" alt="Wavy Architecture" width="400">

For a more detailed explanation, read:
[ARCHITECTURE.md](https://github.com/nots1dd/wavy/blob/main/ARCHITECTURE.md)

## **Security**

We do have a [SECURITY.md](https://github.com/Oinkognito/wavy/blob/main/SECURITY.md).

Since the project is still in **active development**, no absolute promises can be made other than the standards and
practices of the codebase.

## **API References**
Wavy relies on **FFmpeg's core libraries** for processing audio:

- [`libavformat`](https://ffmpeg.org/libavformat.html) - Format handling & demuxing.
- [`libavcodec`](https://ffmpeg.org/libavcodec.html) - Audio decoding & encoding.
- [`libavutil`](https://ffmpeg.org/libavutil.html) - Utility functions for media processing.
- [`libswresample`](https://ffmpeg.org/libswresample.html) - Audio resampling & format conversion.

For detailed API documentation, see:
[APIREF.md](https://github.com/nots1dd/wavy/blob/main/APIREF.md)

## **Dry Run**

A basic walkthrough on how to use Wavy to the fullest! (as of latest commit atleast)

> [!CAUTION]
>
> To understand what is truly happening here,
> read **[Wavy Architecture](https://github.com/Oinkognito/wavy/blob/main/ARCHITECTURE.md)**
>

### Native server (not docker)

Let's say you have an audio file called `testing.mp3` and `cool.flac`. Let us see how you can use Wavy to store them and use for playback.

Firstly, you need to ensure that Wavy is compiled like so:

```bash
make all EXTRA_CMAKE_FLAGS="-D BUILD_EXAMPLES=OFF -D BUILD_FETCHER_PLUGINS=ON -D BUILD_AUDIO_BACKEND_PLUGINS=ON"
```

If that is done, you are good to go!

#### Owner (that's you!)

```bash
./build/wavy_owner -h
```

This should give you the help on all the params you can give to `Wavy Owner`.

For now let's take the most complicated example: You want to store `testing.mp3` (which is of **320 kbps**) as a stream of **128 kbps**.

```bash
# --i  :- Input file
# --o  :- Output directory (of all the HLS segmenting + Transcoding)
# --ip :- Server IP
# --n  :- Your desired nickname that is expected to be unique (exposed to server)
./build/wavy_owner --inputFile=testing.mp3 --outputDir=output --serverIP=127.0.0.1 --nickname=sid123
```

Just run this. It will transcode by default to: **64**, **112**, **128** kbps

> [!NOTE]
>
> You can change what bitrates to transcode to by modifying the
> `bitrates` vector in `wavy/Owner.cc` (line 194). I will come up with a better way
> to do the same in the future.
>

Note that this **WILL** fail if the server is not running on `127.0.0.1` (localhost)

So lets run the **Server** in another terminal.

#### Server

> [!NOTE]
>
> Make sure that you have run either:
>
> ```bash
> # manually give details to generate self signed SSL certificate
> make server-cert
> # or better, just auto-gen it (NO USER INPUT!!)
> make server-cert-gen
> ```

```bash
make run-server
```

That's it! Nothing else to do from your side. (You can view logs if you'd like)

#### Client (can be anyone - even you!)

You can query certain information about what owners are there and all the audio info in the server like so:

1. `/hls/owners`:

Will give details of all the owners registered by the server along with every owner's audio-ids.

```bash
curl -k -X GET https://127.0.0.1:8080/hls/owners
```

Output would look like:

```text
sid123.owner:
  - 5d9f9497-0888-4102-868a-dc4b67f75756
```

2. `/hls/audio-info/`:

Will provide details on each audio-id regardless of owner (will change this for better filtering)

```bash
curl -k -X GET https://127.0.0.1:8080/hls/audio-info/
```

Output will look like:

```text
sid123.owner:
  - 5d9f9497-0888-4102-868a-dc4b67f75756
      1. Title: Passion
      2. Artist: Gabriel Albuquerqüe
      3. Duration: 217 secs
      4. Album: Passion
      5. Bitrate: 322 kbps
      6. Sample Rate: 48000 Hz
      7. Sample Format: fltp
      8. Audio Bitrate: 320 kbps
      9. Codec: mp3
      10. Available Bitrates: [128015,112013,64008,]
```

After querying simple text, let us actually query audio through `WavyClient`.

Again like with `Owner`, run `-h` or `--help` on `wavy_client` binary for more info:

```bash
./build/wavy_client -h
```

Alright lets query the `0th` index of owner `sid123` (which is `5d9f9497-0888-4102-868a-dc4b67f75756`), and I want bitrate stream of **128 kbps** (closest is `128015`).

The default mode to **fetch** the transport streams currently is **`Aggressive`** (more on this later), but just go with it for now. The audio is **NOT** flac, so disable `--playFlac` and finally give the path of the `audioBackendLib` which is always in the format of:

```text
libwavy_audio_backend_<plugin-name>_plugin.so
```

> [!NOTE]
>
> You can give the full path of the `audioBackendLibPath` if needed.
> By default, all plugins will be placed in `<build-dir>/plugins/audio/`.
>

The final command would look like this:

```bash
./build/wavy_client --nickname=sid123.owner --serverIP=127.0.0.1  --bitrate-stream=128015 --fetchMode=Aggressive --index=1 --playFlac=false --audioBackendLibPath=libwavy_audio_backend_PulseAudio_plugin.so
```

> [!NOTE]
>
> If you give a `bitrate-stream` that is not found to be present in the server,
> the **HIGHEST** possible stream will be allotted to the client by default.
>

Now you can see the streams being fetched from the server and the client decodes it followed by playback through `PulseAudio` API. (this will work iff you have PulseAudio in the first place)

## **Examples**

Wavy currently is under heavy development as mentioned above, so for people to understand the project better,
the project maintains an `examples/` directory. Latest `libwavy` APIs are made sure to have atleast one in-depth example

> [!IMPORTANT]
>
> To compile all examples:
>
> ```bash
> make "-DBUILD_EXAMPLES=ON"
> ```
>

Each example should be pretty straightforward as they are isolated API calls to a particular aspect of `libwavy`

## **Documentation**
### **Generating Docs**
Install **Doxygen**, then run:

```bash
doxygen .Doxyfile
xdg-open docs/html/index.html  # Opens the documentation in a browser
```

## **Credits**

1. **TOML++**:  Header-only TOML config file parser and serializer for C++17.  [TOML++ (tomlplusplus)](https://github.com/marzer/tomlplusplus)
2. **Indicators**: Activity Indicators for Modern C++ (17) (**Header-only**) [Indicators](https://github.com/p-ranav/indicators)

## **License**
The **Wavy Project** is licensed under the **BSD-3-Clause License**.
[LICENSE](https://github.com/Oinkognito/wavy/blob/main/LICENSE)
