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
- [**Examples**](#examples)
- [**Server**](#server)
- [**Usage**](#usage)
- [**Documentation**](#documentation)
- [**License**](#license)

## **Introduction**
Wavy is a **lightweight** and **efficient** solution for audio streaming within a local network. It is designed to **encode, decode, and dispatch** audio streams seamlessly while ensuring **secure** data transfer via **SSL/TLS encryption**.

> [!IMPORTANT]
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

### **Optional Dependencies (for Lossless Support)**

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

### **Arch Linux**

```bash
sudo pacman -S --needed \
  ffmpeg base-devel openssl boost tbb-devel zstd cmake make pkgconf \
  libarchive lame flac++
```

### **Ubuntu / Debian**

```bash
sudo apt update && sudo apt install -y \
  ffmpeg build-essential libssl-dev libboost-all-dev libtbb-dev \
  libzstd-dev cmake make pkg-config libarchive-dev \
  libmp3lame-dev libflac++-dev
```

### **Fedora**

```bash
sudo dnf install -y \
    @development-tools gcc-c++ flac-devel boost-devel openssl-devel \
    ffmpeg-free-devel libavcodec-free-devel libavutil-free-devel libavformat-free-devel libswresample-free-devel \
    zstd cmake make pkgconf \
    libarchive-devel lame-devel git wget tbb-devel
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

## **Trial Run**

### Native server (not docker)

## **Examples**

Wavy currently is under heavy development as mentioned above, so for people to understand the project better,
the project maintains an `examples/` directory. Latest `libwavy` APIs are made sure to have atleast one in-depth example 

> [!NOTE]
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

## **License**
The **Wavy Project** is licensed under the **BSD-3-Clause License**.  
[LICENSE](https://github.com/Oinkognito/wavy/blob/main/LICENSE)
