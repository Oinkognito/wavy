#!/bin/bash

set -e

# Colors for better readability
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
BLUE='\033[0;34m'
MAGENTA='\033[0;35m'
CYAN='\033[0;36m'
RESET='\033[0m'

# Unicode decorations
BAR="━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
SECTION="────────────────────────────────────────────────────"

echo -e "${CYAN}${BAR}${RESET}"
echo -e "${MAGENTA}Wavy Build Script v1.0${RESET}"
echo -e "${CYAN}${BAR}${RESET}\n"

# Detect Package Manager
if command -v apt &> /dev/null; then
    PKG_MANAGER="apt"
    UPDATE_CMD="sudo apt update"
    INSTALL_CMD="sudo apt install -y"
elif command -v dnf &> /dev/null; then
    PKG_MANAGER="dnf"
    UPDATE_CMD="sudo dnf check-update"
    INSTALL_CMD="sudo dnf install -y"
elif command -v pacman &> /dev/null; then
    PKG_MANAGER="pacman"
    UPDATE_CMD="sudo pacman -Sy"
    INSTALL_CMD="sudo pacman -S --noconfirm"
elif command -v pkg &> /dev/null; then
    PKG_MANAGER="pkg"
    UPDATE_CMD="pkg update"
    INSTALL_CMD="pkg install -y"
else
    echo -e "${RED}Unsupported package manager. Install dependencies manually.${RESET}"
    exit 1
fi

echo -e "${BLUE}Detected package manager: ${PKG_MANAGER}${RESET}\n"

# List of required packages
REQUIRED_PACKAGES=("cmake" "make" "clang" "pkg-config" "openssl" "zstd" "libarchive" "ffmpeg" "boost")

# Adjust package names for different systems
if [ "$PKG_MANAGER" = "apt" ]; then
    REQUIRED_PACKAGES=("cmake" "make" "clang" "pkg-config" "libssl-dev" "libzstd-dev" "libarchive-dev" "ffmpeg" "libavcodec-dev" "libavformat-dev" "libavutil-dev" "libavfilter-dev" "libswscale-dev" "libswresample-dev" "libboost-all-dev")
elif [ "$PKG_MANAGER" = "dnf" ]; then
    REQUIRED_PACKAGES=("cmake" "make" "clang" "pkg-config" "openssl-devel" "zstd-devel" "libarchive" "ffmpeg" "ffmpeg-devel" "boost-devel")
elif [ "$PKG_MANAGER" = "pacman" ]; then
    REQUIRED_PACKAGES=("cmake" "make" "clang" "pkg-config" "openssl" "zstd" "libarchive" "ffmpeg" "boost")
elif [ "$PKG_MANAGER" = "pkg" ]; then
    REQUIRED_PACKAGES=("cmake" "make" "clang" "pkg-config" "openssl" "libzstd" "libarchive" "ffmpeg" "boost")
fi

echo -e "${BLUE}Checking required dependencies...${RESET}"

MISSING_PACKAGES=()

# Check for each package
for pkg in "${REQUIRED_PACKAGES[@]}"; do
    if ! command -v "${pkg}" &> /dev/null && ! ldconfig -p | grep -q "${pkg}"; then
        echo -e "${RED}Missing: ${pkg}${RESET}"
        MISSING_PACKAGES+=("${pkg}")
    else
        echo -e "${GREEN}Found: ${pkg}${RESET}"
    fi
done

# Install missing packages if applicable
if [ ${#MISSING_PACKAGES[@]} -ne 0 ]; then
    echo -e "\n${YELLOW}The following packages are missing:${RESET}"
    echo -e "${MISSING_PACKAGES[@]}"
    echo -e "\n${BLUE}Updating package lists...${RESET}"
    $UPDATE_CMD
    echo -e "\n${BLUE}Installing missing packages...${RESET}"
    $INSTALL_CMD "${MISSING_PACKAGES[@]}"
fi

echo -e "${GREEN}\nAll dependencies are installed.${RESET}"

echo -e "\n${CYAN}${SECTION}${RESET}"
echo -e "${MAGENTA}Starting Build Process${RESET}"
echo -e "${CYAN}${SECTION}${RESET}\n"

# Ask for build target
echo -e "\n${YELLOW}Enter build target (default: all):${RESET} "
read -r TARGET
TARGET=${TARGET:-all}

# Compile
echo -e "${BLUE}Compiling target: ${TARGET}...${RESET}"
make -j$(nproc) "$TARGET" || { echo -e "${RED}Compilation failed.${RESET}"; exit 1; }

echo -e "\n${CYAN}${BAR}${RESET}"
echo -e "${GREEN}Build successful!${RESET}"
echo -e "${CYAN}${BAR}${RESET}\n"

exit 0
