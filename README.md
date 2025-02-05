# Wavy (ig)

## **DEPENDENCIES**

1. FFmpeg (should come with its libraries)
2. base-devel (g++ / clang++ work as CXX COMPILERS)

## **BUILDING**

To build just run:

```bash 
make encoder # to build encode.cpp
make decoder # to build decode.cpp
make remove # to cleanup all the transport streams and playlists

# To build both decoder and encoder 
make build-all
```

**EXPECTED OUTPUT**:

1. For encoder, it should create a `.m3u8` playlist file that references to every HLS encoded segment (transport stream [.ts] files)

To verify no loss in data over segmentation:

```bash
ffplay output.m3u8 # should have normal expected audio playback
```

> [!NOTE]
> 
> The encoder should work for any audio file except for lossless, we will use FLAC / WAV codecs for it (maybe)
>
> Alternatively, we could transcode WAV/FLAC to AAC (adv audio coding) for HLS compatability
> 

## **API REFERENCES**

Check out `APIref.md`

## **DOCUMENTATION**

Have Doxygen installed.

Run the following commands to view the docs in your browser:

```bash 
doxygen .Doxyfile
xdg-open docs/html/index.html # should open the docs in your browser (default)
```
