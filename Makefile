# Makefile wrapper for CMake build system

# Define the build directory
BUILD_DIR := build
CMAKE := cmake
MAKE := make

# Targets
ENCODER_BIN := hls_segmenter
DECODER_BIN := hls_decoder
SERVER_BIN := hls_server
DISPATCHER_BIN := hls_dispatcher
PLAYBACK_BIN := hls_playback
CLIENT_BIN := hls_client

# Default target: Build everything
default: all

# Parameterized configure function
define configure
	@mkdir -p $(BUILD_DIR)
	@cd $(BUILD_DIR) && $(CMAKE) -DCMAKE_EXPORT_COMPILE_COMMANDS=1 -DBUILD_TARGET="$(1)" -DCMAKE_BUILD_TYPE="$(2)" ..
endef

# Build all targets
all:
	$(call configure,All,Release)
	@$(MAKE) -C $(BUILD_DIR)

# Build individual components
encoder:
	$(call configure,Encoder Only,Release)
	@$(MAKE) -C $(BUILD_DIR) $(ENCODER_BIN)

decoder:
	$(call configure,Decoder Only,Release)
	@$(MAKE) -C $(BUILD_DIR) $(DECODER_BIN)

playback:
	$(call configure,Playback Only,Release)
	@$(MAKE) -C $(BUILD_DIR) $(PLAYBACK_BIN)

server:
	$(call configure,Server Only,Release)
	@$(MAKE) -C $(BUILD_DIR) $(SERVER_BIN)

dispatcher:
	$(call configure,Dispatcher Only,Release)
	@$(MAKE) -C $(BUILD_DIR) $(DISPATCHER_BIN)

client:
	$(call configure,Client Only,Release)
	@$(MAKE) -C $(BUILD_DIR) $(CLIENT_BIN)

# Enable verbose build
verbose:
	$(call configure,All,Verbose)
	@$(MAKE) -C $(BUILD_DIR)

# Code formatting
format:
	@clang-format -i src/*.cpp include/*.hpp

# Code linting/fixing
tidy:
	@clang-tidy -fix src/*.cpp include/*.hpp --

# Clean build files
clean:
	@rm -rf $(BUILD_DIR)

cleanup:
	@rm *.ts *.m3u8

server-cert:
	@openssl req -x509 -newkey rsa:4096 -keyout server.key -out server.crt -days 365 -nodes

.PHONY: default all encoder decoder server dispatcher verbose clean
