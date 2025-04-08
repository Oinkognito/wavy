# Architectural Components of Wavy

> [!IMPORTANT]
> 
> Wavy will always interface with the port number: **8080**!!!
> 
> So whenever `port no` is mentioned, assume that we are talking about 8080.
> 

## Owner

Owner is the client that has the audio file that is ready to send the segments of the audio file to the server. 

It consists of:

1. **Encoder** (A global transcoder + HLS segmenter)
2. **Dispatcher** (organizes the segments and dispatches to the server)

#### What is happening here?

Let us say you have an audio file `song.mp3` or `song.flac` (currently Wavy only supports `MP3` for lossy and `FLAC` for lossless).

The first step for the encoder is to check if it has a transcoding job to do. What is that? TLDR; if you want to change the bitrate of an audio file, significant data is changed and resampled in order for the bitrate to be changed. Decreasing the bitrate also decreases the file size, quality and vice versa.

The possible modes of transcoding that are tested and supported by Wavy:

1. FLAC -> mp3 
2. mp3 -> mp3 

> [!NOTE]
> 
> You can try for .wav files as well by using FLAC++
> to convert .wav to .flac files and then transcoding.
> 
> Ideally, most if not all lossy file formats and containers 
> can be transcoded into mp3 but that has not been tested 
> extensively.
> 

Why are we transcoding? This greatly helps in ABR (adaptive bit rate) streaming for receivers and creates a lot of options for users to download a specific bitrate stream.

Transcoding and HLS (HTTPS Live Streaming) is done using the FFmpeg libraries (formaly known as libav) so we **NOT** using the binary in any way. This is done to ensure faster API access without having to rely on running a binary and proper bindings for more compatibility with Wavy and oneTBB (used for parallized transcoding)

The binary for running the encoder is like so:

```bash 
./build/hls_encoder

# using makefile 
make run-encoder ARGS="<input file> <output dir> <audio format> [--debug]"
```

It should give a prompt like so:

```bash 
[2025-04-08 07:17:18.814] [ERROR]   [encode.cpp:123 - main] Usage: ./build/hls_encoder <inpu
t file> <output directory> <audio format> [--debug]
```

It is pretty self explanatory but you need an input file (audio file) an output directory for the segments to go in.

The audio format is to ensure that you want to preserve your lossless audio (FLAC). So let us say you ONLY want to HLS segment your pure FLAC data without transcoding, this can be done using `--flac` audio format flag. By default (without `--flac` flag), this will transcode any give input file into an `mp3` file of `X` bitrate and then create hls segments of said bitrate.

> [!NOTE]
> 
> Currently the range of bitrates for transcoding is allotted by 
> a `vector<int> bitrates` variable which has `64`, `112` and `128` kbps as default for now.
> 
> In the future, we intend on creating the logic for dynamic bitrate assorting based on the given 
> input file's bitrate.
> 

What is HLS segmenting? 

From this point onward, a `.m3u8` file is called as a **playlist** file; we will talk about this later.

The audio file is split into encoded transport streams (.ts) for `mp3` and `.m4s` for `flac` files. For simplicity, let us call these as **segments** from now on. 

Each segment's path is referened inside a playlist file so it is like a register that (for that specfic audio file) where its segments are located and some other details like each segments duration and codec.

Diagrammatically it can thought of like this:

```text
+-------------------------------+
|    playlist_<bitrate>.m3u8    |
|-------------------------------|
| #EXTM3U                       |
| #EXT-X-VERSION:3              |
| #EXT-X-TARGETDURATION:10      |
| #EXT-X-MEDIA-SEQUENCE:0       |
|                               |                                 +-------------------+
| #EXTINF:10.0,                 |                                 |  segment0.ts      |
| segment0.ts                   | ------------------------------> |                   |
| #EXTINF:10.0,                 |                                 |        10s        |
| segment1.ts                   | -------------                   +-------------------+
| #EXTINF:10.0,                 |             | 
| segment2.ts                   |             | 
| ...                           |             ------------------> +-------------------+
| #EXT-X-ENDLIST                |                                 | segment1.ts       | 
+-------------------------------+                                 |                   |
                                                                  |        10s        |
                                                                  +-------------------+

                                                            (so on for the remaining segments ...)
```

Pretty simple concept, but there is another level above just a regular playlist file called a **Master Playlist**. This refers to a playlist file that has path references to other playlist files NOT segments like we saw before!

The master playlist is an easy and conventional way to maintain playlists that hold segments of different audio quality (in our case different bitrates) that can be referenced without a hassle. This can be understood by the following diagram:

```text 
+------------------------------------+                            
|           master.m3u8              |                            
|------------------------------------|
| #EXTM3U                            |            
| #EXT-X-STREAM-INF:BANDWIDTH=64000  |            
| low.m3u8                           |
| #EXT-X-STREAM-INF:BANDWIDTH=128000 |
| medium.m3u8                        |
| #EXT-X-STREAM-INF:BANDWIDTH=256000 |
| high.m3u8                          |
+------------------------------------+
        
        |               |                | 
        |               |                |
        v               v                v

+-------------------+ +-------------------+ +------------------+
|    low.m3u8       | |   medium.m3u8     | |   high.m3u8      |
|-------------------| |-------------------| |------------------|
| #EXTINF:10.0,     | | #EXTINF:10.0,     | | #EXTINF:10.0,    |
| seg0_low.ts       | | seg0_med.ts       | | seg0_high.ts     |
| #EXTINF:10.0,     | | seg1_med.ts       | | seg1_high.ts     |
| seg1_low.ts       | | ...               | | ...              |
| ...               | |                   | |                  |
+-------------------+ +-------------------+ +------------------+

        |                   |                   |
        v                   v                   v

  +-------------+     +-------------+     +-------------+
  | seg0_low.ts |     | seg0_med.ts |     | seg0_high.ts|
  +-------------+     +-------------+     +-------------+
  | seg1_low.ts |     | seg1_med.ts |     | seg1_high.ts|
  +-------------+     +-------------+     +-------------+
        ..                 ..                  ..
   (low segments)     (medium segments)   (high segments)  
```

It is like a parent playlist, the diagram should be more than enough to understand what is going on here.

The binary for running the dispatcher is like so:

```bash 
./build/hls_dispatcher

# using makefile 
make dispatch ARGS="<server-ip> <port no> <payload-dir> <master-playlist-name>"
```

It should give a prompt like so:

```bash 
[2025-04-08 08:03:04.309] [ERROR]   [dispatcher.cpp:38 - main] Usage: ./build/hls_dispatcher
 <server> <port> <payload-directory> <master_playlist>
```

This handles a lot of things actually:

1. Locates all the playlists and respective segments
2. If the job is not of a `FLAC` file, then apply a slightly modified ZSTD compression on each file
3. Move all the targets to another directory that is now the **PAYLOAD** directory that contains the data to be shipped
4. Compress the entire payload directory into a single GNU tar file to be shipped 
5. Finally, dispatch the tar file over to the Wavy Server (also known as HLS server currently) over SSL/TLS encrypted LAN connection

```text 
Step 1: Locate Playlists and Segments
+--------------------------+
|     Source Directory     |
|--------------------------|
|  - master.m3u8           |
|  - low.m3u8              |
|  - medium.m3u8           |
|  - high.m3u8             |
|  - seg0.ts               |
|  - seg1.ts               |
|  - ...                   |
+--------------------------+
              |
              v

Step 2: Compress (if not FLAC)
+----------------------------------+
| Modified ZSTD Compression Check |
|----------------------------------|
|  [✓] If file is NOT FLAC         |
|      → Apply ZSTD compression    |
|  [✗] If FLAC → leave as-is       |
|  Result: seg0.ts.zst, etc.       |
+----------------------------------+
              |
              v

Step 3: Move Files to PAYLOAD Directory
+---------------------------+
|     PAYLOAD Directory     |
|---------------------------|
|  - master.m3u8            |
|  - low.m3u8               |
|  - medium.m3u8            |
|  - high.m3u8              |
|  - seg0.ts.zst / seg0.m4s | (m4s is a container that contains lossless hls segments of a FLAC file)
|  - seg1.ts.zst / seg1.m4s |
|  - ...                    |
+--------------------------+
              |
              v

Step 4: Create TAR Archive
+----------------------------+
|     payload.tar.gz         |
|----------------------------|
|  <- Contents of PAYLOAD →  |
+----------------------------+
              |
              v

Step 5: Dispatch to Wavy Server
+-----------------------------------------------------+
|    Upload over SSL/TLS to LAN-based HLS Server      |
|-----------------------------------------------------|
|  Using secure HTTPS POST to Wavy Server             |
|  → Endpoint: https://<server-ip>:port/hls/upload    |
+-----------------------------------------------------+
```

The dispatcher does quite a few jobs, currently we are considering of adding a new `in-between` component between the dispatcher and owner 
but nothing too conclusive yet.

Why are we compressing data? Simple. It saves time to transfer as the data size is ultimately less.

The reason for why we went for ZSTD compression is quite straightforward and better explained here in [dispatch header](https://github.com/oinkognito/wavy/blob/main/libwavy/dispatch/entry.hpp) along with some FAQs (will update it in this file soon)

## **Server**

The **Wavy-Server** allows **secure** transport stream handling over HTTPS.

The Server Storage Organization is indexed below: 

```text 
hls_storage/
├── 192.168.1.10/                                    # IP Address 192.168.1.10 (example)
│   ├── 1435f431-a69a-4027-8661-44c31cd11ef6/        # Randomly generated audio id
│   │   ├── index.m3u8
│   │   ├── hls_mp3_64.m3u8                          # HLS MP3 encoded playlist (64-bit)
│   │   ├── hls_mp3_64_0.ts                          # First transport stream of hls_mp3_64 playlist                
│   │   ├── ...                                      # Similarly for 128 and 256 bitrates
│   │   ├── metadata.toml                            # Metadata and other song information
│   ├── e5fdeca5-57c8-47b4-b9c6-60492ddf11ae/
│   │   ├── index.m3u8
│   │   ├── hls_flac_64.m3u8                         # HLS FLAC encoded playlist (64-bit)
│   │   ├── hls_flac_64_0.ts                         # First transport stream of hls_mp3_64 playlist 
│   │   ├── ...                                      # Similarly for 128 and 256 bitrates
│   │   ├── metadata.toml                            # Metadata and other song information
│    
```

The server does not delete these indices after the server dies. The server allows for **PERSISTENT STORAGE**.

This makes it so that every owner can index multiple audio files under a clean directory structure that is logical to query and playback.

So the capability of the server totally depends on **YOUR** filesystem. This gives you full power to manage your server library to the fullest.

**Current Routes**:

1. `/hls/clients`: Gives a neat hierarchial structure of each Owner-IP-ID with their uploaded audio ids.
2. `/hls/audio-info/`: Provides a neat hierarchial structure of every Audio ID's provided metadata (from their uploaded metadata.toml)
3. `/hls/ping`: A basic route to check if the server is "alive" (sends `pong` if running)

If you want to get the metadata for a single audio-id, you can always just query it like so:

```bash
curl -k -X GET https://<server-ip>:8080/<ip-id>/<audio-id>/metadata.toml 
```

> [!NOTE]
> 
> The Server Architecture and Organization is made in such a manner
> that you will **NEVER** require FFmpeg libraries to be present.
> 
> This reduces any dependencies and complications in your server,
> and overall reduces load of operations.
> 
> It will only require the following:
> 
> 1. Standard C/C++ libs (should be there already)
> 2. Boost C++ (preferably above 1.70)
> 3. OpenSSL 
> 4. ZSTD 
> 

### **Flexibility**

The architecture is designed in a way to make it more flexible in the future.

The server's importance in the overall flow of operations is always kept at a minimum, to ensure that if we were to implement a P2P solution for this someday, the transition and implementation would lead to no heads being bashed into a wall.

### **Generating SSL Certificates**
To generate a **self-signed certificate**, run:

```bash
openssl req -x509 -newkey rsa:4096 -keyout server.key -out server.crt -days 365 -nodes
```

Or use the **Makefile shortcut**:

```bash
make server-cert
```

> [!NOTE] 
> 
> Place `server.crt` and `server.key` in the **current working directory** before starting the server.
> 

> [!WARNING] 
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
make run-server
```

It will:
1. Accept secure `.m3u8` & `.ts` uploads.  
2. Assign **UUIDs** to clients for session tracking.  
3. Serve stored playlists via `GET /hls/<ip-id>/<client_id>/<filename>`.

### **Uploading a Playlist**
To upload a **compressed HLS playlist**:

```bash
curl -X POST -F "file=@playlist.tar.gz" https://localhost:8080/upload -k
```

### **Fetching a Client List**
```bash
curl https://localhost:8443/hls/clients -k
```

## Client 

Coming soon
