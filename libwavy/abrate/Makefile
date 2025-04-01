CPP := g++
SRC := main.cpp 
LIBS := -lboost_log -lboost_log_setup -DBOOST_LOG_DLL -lboost_filesystem -lboost_system -lssl -lcrypto
BIN := abr-bin

abr:
	$(CPP) -o $(BIN) $(SRC) $(LIBS)
