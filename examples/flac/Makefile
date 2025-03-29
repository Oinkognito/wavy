# Compiler and Flags
CXX      := g++
CXXFLAGS := -std=c++20 -Wall -Wextra -Werror -I../../codecs/flac
LDFLAGS  := -lFLAC++ -DBOOST_LOG_DLL -lboost_log -lboost_log_setup -lboost_system -lboost_filesystem -pthread

# Source Files
SRC  := main.cpp
OUT  := flac_metadata

# Build Target
all: $(OUT)

$(OUT): $(SRC)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

# Clean Build Files
clean:
	rm -f $(OUT)
