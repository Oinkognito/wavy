CPP := c++
VERBOSE_FLAG := VERBOSE=1
PKG_CONFIG := pkg-config
DEPS := $(shell $(PKG_CONFIG) --libs --cflags libavutil libavformat libavcodec libswresample)
BIN_1 := hls_segmenter
BIN_2 := hls_decoder
SERVER_BIN := hls_server
SRC_1 := encode.cpp
SRC_2 := decode.cpp
SERVER := server.cpp
DISPATCHER := dispatcher.cpp
DISPATCHER_BIN := hls_dispatcher
BOOST_LIBS := -lboost_log -lboost_log_setup -lboost_system -lboost_thread -lboost_filesystem -lboost_date_time -lboost_regex -lpthread -lssl -lcrypto
LIB_ARCHIVE_LIBS := -larchive
SAMPLE_AUDIO_1 := sample1.mp3
OUTPUT_SAMPLE_PLAYLIST := sample1.m3u8
CXXFLAGS = -std=c++20 -Wall -Wextra -O2

all: build-all

encoder: $(SRC_1)
	$(CPP) $(SRC_1) $(DEPS) -o $(BIN_1)

decoder: $(SRC_2)
	$(CPP) $(SRC_2) $(DEPS) -o $(BIN_2)

run-encoder:
	./$(BIN_1) $(SAMPLE_AUDIO_1) $(OUTPUT_SAMPLE_PLAYLIST) 

server:
	$(CPP) -DBOOST_LOG_DYN_LINK $(SERVER) -o $(SERVER_BIN) $(BOOST_LIBS) $(LIB_ARCHIVE_LIBS) $(CXXFLAGS)

dispatcher:
	$(CPP) -DBOOST_LOG_DYN_LINK $(DISPATCHER) -o $(DISPATCHER_BIN) $(BOOST_LIBS) $(LIB_ARCHIVE_LIBS)

tidy:
	clang-format -i *.cpp

build-all: build-encoder build-decoder

remove:
	rm -f *.ts *.m3u8

clean:
	rm -f $(BIN_1) $(BIN_2) $(SERVER_BIN) *.ts *.m3u8
