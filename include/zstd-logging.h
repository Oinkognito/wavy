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
