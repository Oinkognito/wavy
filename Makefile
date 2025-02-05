CPP := c++
VERBOSE_FLAG := VERBOSE=1
PKG_CONFIG := pkg-config
DEPS := $(shell $(PKG_CONFIG) --libs --cflags libavutil libavformat libavcodec libswresample)
BIN_1 := hls_segmenter
BIN_2 := hls_decoder
SRC_1 := encode.cpp
SRC_2 := decode.cpp
SAMPLE_AUDIO_1 := sample1.mp3
OUTPUT_SAMPLE_PLAYLIST := sample1.m3u8

all: build-all

encoder: $(SRC_1)
	$(CPP) $(SRC_1) $(DEPS) -o $(BIN_1)

decoder: $(SRC_2)
	$(CPP) $(SRC_2) $(DEPS) -o $(BIN_2)

run-encoder:
	./$(BIN_1) $(SAMPLE_AUDIO_1) $(OUTPUT_SAMPLE_PLAYLIST) 

build-all: build-encoder build-decoder

remove:
	rm -f *.ts *.m3u8

clean:
	rm -f $(BIN_1) $(BIN_2) *.ts *.m3u8
