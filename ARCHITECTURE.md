# HLS Streaming System Architecture

> [!IMPORTANT]
> 
> This is outdated!
> 

## Overview

In this architecture model, there are 3 main components which various jobs:

**Owner Client** --> **Client** --> **Receiver Client**

1. **Client (Encoder & Dispatcher)** – Encodes audio files into HLS segments and dispatches them to the server in a singular compressed file.
2. **Server** – Receives, validates, and hosts the HLS segments, exposing them over LAN.
3. **Receiver Client (ABR & Grabber & Decoder)** – Requests and plays the HLS stream (with ABR logic possibly), with support for Adaptive Bitrate Streaming (ABR) and performs real-time playback of the expected transport stream segment

## Current Architecture
### 1. Client (Encoder & Dispatcher)
- The client takes an input audio file and encodes it into HLS segments.
- Currently, the system supports only **lossy audio** formats (e.g., MP3) due to the lack of FLAC/WAV codecs.
- The encoder outputs a master playlist (`.m3u8`), which has references to multiple bitrate playlists (eg., hls_64.m3u8 -> 64 bitrate) and multiple segment files (`.ts`).
- The dispatcher sends these files to the server using a structured protocol that ensures data integrity.

### 2. Server
- Receives the HLS segments and playlists from the client.
- Performs extraction and validation checks on incoming data before storing it.
- Exposes the HLS content over LAN so that other clients can request playback.
- Currently, the server **does not implement ABR** or **network diagnostics**, meaning it serves only the originally encoded quality.

> [!IMPORTANT]
> 
> The decision of implementing ABR logic is still undecided whether to put it in the server 
> or the receiver client.
> 
> From most applications like VLC, MPV, etc. it seems that the receiver performs diagnostics
> and decode the requested bitrate transport streams from the server 
> 

### 3. Receiver (Client Player)
- A separate client that requests the HLS playlist and segments from the server.
- Parses the `.m3u8` playlist and streams the `.ts` segments in real time.
- Can play the lossy streams, but has no ability to switch between bitrates dynamically.

> [!NOTE]
> 
> You can test out the receving part (that has ABR) using VLC/MPV or ffplay (network streams)
> 
> The above applications have custom decoding logic that keep ABR into account and request the server 
> for transport streams of various bitrate playlists depending on the network conditions 
> 

## Intended Architecture
### 1. Enhancements to the Client (Encoder & Dispatcher)
- **Support for FLAC and WAV codecs** to improve compatibility with lossless formats.
- **Multi-bitrate encoding:** The encoder will generate multiple quality levels of segments, making them available for ABR.
- **Improved network handling** to optimize dispatching efficiency based on server response and congestion status.

### 2. Server Improvements
- **Adaptive Bitrate Streaming (ABR) Support:** The server will analyze receiver-server network conditions and adjust the stream quality dynamically. [might move this logic to receiver client]
- **More robust validation:** Ensure segments are correctly formatted and complete before making them available.
- **Enhanced networking features:** Load balancing, congestion control, and buffer management for smoother playback.

### 3. Receiver Upgrades
- **ABR capability:** The receiver should be able to switch between available bitrates based on bandwidth availability.
- **Setup Playback:** After network diagnostics and performs ABR to grab the required transport streams, have real-time optimized playback.
- **Optimized playback:** Improve buffering and caching mechanisms to reduce playback interruptions.

## Future Goals
- Implement **secure transmission** using SSL/TLS for segment transfers.
- Add **error correction** for unreliable network conditions.
- Extend the system to support **mobile and embedded device clients** for a more flexible streaming experience.

## Future

This architecture can be made more flexible due to the way the Owner transfers to the server.

**OWNER** ----(Compressed file with abr-hls segmentation logic)-----> **SERVER** ----(Can request either required transport stream / compressed encoded file)---> **RECEIVER**

In case the `Receiver` wants to become the new owner (through a `CHOWN` request) the `Receiver` now requests the gzip file and upon receving it, can be decoded back to the audio file (metadata might be a problem in this case) and can distribute this file to other users on the same network.

This reduces dependency on the `Owner` and overall usability of the application.

Maybe we can introduce a direct P2P transfer of the audio file (original) from `Owner` -> `Receiver` to further propel this. (Over LAN)
