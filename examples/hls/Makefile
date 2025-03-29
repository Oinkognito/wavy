CC := g++
LIBAV := -lavformat -lavutil -lswresample -lavcodec 
LIB_BOOST := -DBOOST_LOG_DLL -lboost_log -lboost_log_setup -lboost_system -lboost_thread

SRC := main.cpp

BIN := make_hls 

MAKE := make

all:
	$(MAKE) hls

hls:
	$(CC) $(SRC) -o $(BIN) $(LIBAV) $(LIB_BOOST)

clean:
	@rm -f $(BIN)
