/************************************************
 * Wavy Project - High-Fidelity Audio Streaming
 * ---------------------------------------------
 * 
 * Copyright (c) 2025 Oinkognito
 * All rights reserved.
 * 
 * This source code is part of the Wavy project, an advanced
 * local networking solution for high-quality audio streaming.
 * 
 * License:
 * This software is licensed under the BSD-3-Clause License.
 * You may use, modify, and distribute this software under
 * the conditions stated in the LICENSE file provided in the
 * project root.
 * 
 * Warranty Disclaimer:
 * This software is provided "AS IS," without any warranties
 * or guarantees, either expressed or implied, including but
 * not limited to fitness for a particular purpose.
 * 
 * Contributions:
 * Contributions to this project are welcome. By submitting 
 * code, you agree to license your contributions under the 
 * same BSD-3-Clause terms.
 * 
 * See LICENSE file for full details.
 ************************************************/

#ifndef ZSTD_COMPRESSION_LOGGING_H
#define ZSTD_COMPRESSION_LOGGING_H

#include <stdio.h>

#define ANSI_C_RESET  "\033[0m\033[39m\033[49m" // Reset all styles and colors
#define ANSI_C_BOLD   "\033[1m"                 // Bold text
#define ANSI_C_RED    "\033[38;5;124m"          // Gruvbox Red (#cc241d)
#define ANSI_C_GREEN  "\033[38;5;142m"          // Gruvbox Green (#98971a)
#define ANSI_C_YELLOW "\033[38;5;214m"          // Gruvbox Yellow (#d79921)
#define ANSI_C_BLUE   "\033[38;5;109m"          // Gruvbox Blue (#458588)
#define ANSI_C_CYAN   "\033[38;5;108m"          // Gruvbox Aqua/Cyan (#689d6a)
#define ANSI_C_WHITE  "\033[38;5;223m"          // Gruvbox FG1 (#ebdbb2)

// ─────────────────────────── LOGGING MACROS ─────────────────────────────

/*
 * WE DONT REALLY NEED THIS LOGGING UNLESS ABSOLUTELY NECESSARY
 */ 

#ifdef WAVY_ZSTD_DEBUG
#define ZSTD_LOG_INFO(fmt, ...) printf("[INFO] " fmt "\n", ##__VA_ARGS__)

#define ZSTD_LOG_WARN(fmt, ...) printf(ANSI_C_YELLOW "[WARN] " ANSI_C_RESET fmt "\n", ##__VA_ARGS__)

#define ZSTD_LOG_ERROR(fmt, ...) printf(ANSI_C_RED "[ERROR] " ANSI_C_RESET fmt "\n", ##__VA_ARGS__)

#define ZSTD_LOG_SUCCESS(fmt, ...) \
  printf(ANSI_C_GREEN "[SUCCESS] " ANSI_C_RESET fmt "\n", ##__VA_ARGS__)

#define ZSTD_LOG_START_SECTION(title)                                       \
  printf(ANSI_C_BLUE "\n━━━━━━━━━━━━━━━━━ %s ━━━━━━━━━━━━━━━━━\n"             \
                   ANSI_C_RESET, \
         title)

#define ZSTD_LOG_END_SECTION() printf(ANSI_C_BLUE "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" ANSI_C_RESET)
#else
    #define ZSTD_LOG_INFO(fmt, ...) ((void)0) // No-op for release
    #define ZSTD_LOG_WARN(fmt, ...) ((void)0) // No-op for release
    #define ZSTD_LOG_ERROR(fmt, ...) ((void)0) // No-op for release
    #define ZSTD_LOG_SUCCESS(fmt, ...) ((void)0) // No-op for release
    #define ZSTD_LOG_START_SECTION(title) ((void)0) // No-op for release
    #define ZSTD_LOG_END_SECTION() ((void)0) // No-op for release
#endif 

#endif // ZSTD_COMPRESSION_LOGGING_H
