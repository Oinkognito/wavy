# Makefile wrapper for CMake build system of Wavy

# Define the build directory
BUILD_DIR := build
CMAKE := cmake
MAKE := make

# Targets
OWNER_BIN := wavy_owner
SERVER_BIN := wavy_server
CLIENT_BIN := wavy_client

# Allow extra flags for CMake
EXTRA_CMAKE_FLAGS ?=

# Default target: Build everything
default: all

# Parameterized configure function with optional extra flags
define configure
	@mkdir -p $(BUILD_DIR)
	@cd $(BUILD_DIR) && $(CMAKE) -DCMAKE_EXPORT_COMPILE_COMMANDS=1 -DBUILD_TARGET="$(1)" -DCMAKE_BUILD_TYPE="$(2)" $(EXTRA_CMAKE_FLAGS) ..
endef

# Initialize dependencies
init:
	@wget -O $(TOMLPP_DEST_DIR)/toml.hpp $(TOMLPP_URL)

# Build all targets
all:
	$(call configure,All,Release)
	@$(MAKE) -C $(BUILD_DIR)

rebuild:
	$(call configure,All,Release)
	@$(MAKE) -C $(BUILD_DIR)

# Build individual components
owner:
	$(call configure,Owner Only,Release)
	@$(MAKE) -C $(BUILD_DIR) $(OWNER_BIN)

server:
	$(call configure,Server Only,Release)
	@$(MAKE) -C $(BUILD_DIR) $(SERVER_BIN)

client:
	$(call configure,Client Only,Release)
	@$(MAKE) -C $(BUILD_DIR) $(CLIENT_BIN)

# Enable verbose build
verbose:
	$(call configure,All,Verbose)
	@$(MAKE) -C $(BUILD_DIR)

# Code formatting
format:
	@find src libwavy examples libquwrof examples -type f \( -name "*.cpp" -o -name "*.hpp" \) ! -name "toml.hpp" -exec clang-format -i {} +

# Code linting/fixing
tidy:
	@find src libwavy libquwrof -type f \( -name "*.cpp" -o -name "*.hpp" \) ! -name "toml.hpp" | xargs clang-tidy -p $(BUILD_DIR)

prepend-license-src:
	@find src libwavy libquwrof examples -type f \( -name "*.cpp" -o -name "*.hpp" -o -name "*.h" \) ! -name "toml.hpp" -exec ./scripts/license-prepend.sh {} +

# Clean build files
clean:
	@rm -rf $(BUILD_DIR)
	@echo -e "-- Cleaned up the project's '$(BUILD_DIR)' directory"

cleanup:
	@rm -f *.ts *.m3u8
	@echo -e "-- Removed files matching regex `*.m3u8 and *.ts`."

run-server:
	./$(BUILD_DIR)/$(SERVER_BIN)

run-owner:
	./$(BUILD_DIR)/$(OWNER_BIN) $(ARGS)

server-cert:
	@openssl req -x509 -newkey rsa:4096 -keyout server.key -out server.crt -days 365 -nodes

# Generates a default certificate and private key (no user input)
server-cert-gen:
	@openssl req -x509 -newkey rsa:4096 -keyout server.key -out server.crt -days 365 -nodes -subj "/CN=localhost"

.PHONY: default all owner server client run-owner run-server verbose clean cleanup format tidy init server-cert server-cert-gen
