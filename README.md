# **Wavy**

A **local networking solution** for **audio streaming and sharing**, supporting **lossless and lossy audio formats** through a unified platform.

## **Table of Contents**
- [**Introduction**](#introduction)
- [**Dependencies**](#dependencies)
- [**Building**](#building)
- [**Architecture**](#architecture)
- [**API References**](#api-references)
- [**Server**](#server)
- [**Usage**](#usage)
- [**Documentation**](#documentation)
- [**License**](#license)

---

## **Introduction**
Wavy is a **lightweight** and **efficient** solution for audio streaming within a local network. It is designed to **encode, decode, and dispatch** audio streams seamlessly while ensuring **secure** data transfer via **SSL/TLS encryption**.

It supports:
- **Lossless formats** (FLAC, ALAC, WAV)
- **Lossy formats** (MP3, AAC, Opus, Vorbis)
- **Adaptive bitrate streaming** using **HLS (HTTP Live Streaming)**.
- **Metadata extraction** and **TOML-based** configuration.
- **Transport stream decoding** via **FFmpeg** for real-time audio playback.

---

## **Dependencies**
To build and run **Wavy**, install the following dependencies:

| Dependency  | Purpose |
|-------------|---------|
| **FFmpeg**  | Audio encoding, decoding, and streaming |
| **Base-devel** | Includes `g++` / `clang++` for C++ compilation |
| **OpenSSL** | Secure communication using SSL/TLS |
| **Boost** | Asynchronous networking & utility functions |
| **libzstd** | Lossless compression (Zstandard) |
| **CMake & Make** | Build system tools |
| **Libarchive** | Handling `.tar`, `.gz`, `.zst` compressed files |

> **Note:** Ensure that **FFmpeg** is compiled with `libavformat`, `libavcodec`, `libavutil`, and `libswresample`.

---

## **Building**
To compile the different components, run:

```bash
make encoder     # Builds encode.cpp
make decoder     # Builds decode.cpp
make dispatcher  # Builds dispatcher for stream management
make server      # Builds the Wavy streaming server
make remove      # Cleans up all generated transport streams and playlists

make all         # Builds all components at once
```

---

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

---

## **API References**
Wavy relies on **FFmpeg's core libraries** for processing audio:

- [`libavformat`](https://ffmpeg.org/libavformat.html) - Format handling & demuxing.
- [`libavcodec`](https://ffmpeg.org/libavcodec.html) - Audio decoding & encoding.
- [`libavutil`](https://ffmpeg.org/libavutil.html) - Utility functions for media processing.
- [`libswresample`](https://ffmpeg.org/libswresample.html) - Audio resampling & format conversion.

For detailed API documentation, see:  
[APIREF.md](https://github.com/nots1dd/wavy/blob/main/APIREF.md)

---

## **Server**
The **Wavy-Server** allows **secure** transport stream handling over HTTPS.

### **Generating SSL Certificates**
To generate a **self-signed certificate**, run:

```bash
openssl req -x509 -newkey rsa:4096 -keyout server.key -out server.crt -days 365 -nodes
```

Or use the **Makefile shortcut**:

```bash
make server-cert
```

> ![NOTE] 
> 
> Place `server.crt` and `server.key` in the **current working directory** before starting the server.
> 

> ![WARNING] 
> 
> This is a **self-signed certificate**.  
> - Use `-k` flag in **cURL** to bypass SSL validation.
> - Accept the self-signed certificate when using **VLC/MPV**.
> 

---

## **Usage**
### **Starting the Server**
Once compiled, start the server:

```bash
./build/hls_server
```

It will:
1. Accept secure `.m3u8` & `.ts` uploads.  
2. Assign **UUIDs** to clients for session tracking.  
3. Serve stored playlists via `GET /hls/<client_id>/<filename>`.

### **Uploading a Playlist**
To upload a **compressed HLS playlist**:

```bash
curl -X POST -F "file=@playlist.tar.gz" https://localhost:8443/upload -k
```

### **Fetching a Client List**
```bash
curl https://localhost:8443/hls/clients -k
```

---

## **Documentation**
### **Generating Docs**
Install **Doxygen**, then run:

```bash
doxygen .Doxyfile
xdg-open docs/html/index.html  # Opens the documentation in a browser
```

## **License**
The **Wavy Project** is licensed under the **BSD-3-Clause License**.  
[LICENSE](https://github.com/Oinkognito/wavy/blob/main/LICENSE)
