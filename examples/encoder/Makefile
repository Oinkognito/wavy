CC := g++
LIBAV := -lavformat -lavutil -lswresample -lavcodec
FLAC := -lFLAC++

ENCODE_MP3_SRC := encode.cpp
ENCODE_FLAC_SRC := encode-flac.cpp

BIN_1 := encode_mp3 
BIN_2 := encode_flac

MAKE := make

all:
	$(MAKE) mp3
	$(CC) $(ENCODE_FLAC_SRC) -o $(BIN_2) $(LIBAV) $(FLAC)

mp3:
	$(CC) $(ENCODE_MP3_SRC) -o $(BIN_1) $(LIBAV)

