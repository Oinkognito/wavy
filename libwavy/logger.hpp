#pragma once
/********************************************************************************
 *                                Wavy Project                                  *
 *                         High-Fidelity Audio Streaming                        *
 *                                                                              *
 *  Copyright (c) 2025 Oinkognito                                               *
 *  All rights reserved.                                                        *
 *                                                                              *
 *  License:                                                                    *
 *  This software is licensed under the BSD-3-Clause License. You may use,      *
 *  modify, and distribute this software under the conditions stated in the     *
 *  LICENSE file provided in the project root.                                  *
 *                                                                              *
 *  Warranty Disclaimer:                                                        *
 *  This software is provided "AS IS", without any warranties or guarantees,    *
 *  either expressed or implied, including but not limited to fitness for a     *
 *  particular purpose.                                                         *
 *                                                                              *
 *  Contributions:                                                              *
 *  Contributions are welcome. By submitting code, you agree to license your    *
 *  contributions under the same BSD-3-Clause terms.                            *
 *                                                                              *
 *  See LICENSE file for full legal details.                                    *
 ********************************************************************************/

#include <boost/algorithm/string/regex.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/sinks.hpp>
#include <boost/log/sinks/text_file_backend.hpp>
#include <boost/log/sources/logger.hpp>
#include <boost/log/sources/severity_logger.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/regex.hpp>
#include <boost/thread.hpp>

#include <libwavy/common/api/entry.hpp>

/*
 * LOGGER
 *
 * This is a logging header that neatly wraps around boost logger
 *
 * Gives up-to-date logs with SeverityLevel and time information
 *
 * Can also in the future write these to a file
 *
 */

// Force ANSI Colors (Ignoring Terminal Themes)
#define RESET  "\033[0m\033[39m\033[49m" // Reset all styles and colors
#define BOLD   "\033[1m"                 // Bold text
#define RED    "\033[38;5;124m"          // Gruvbox Red (#cc241d)
#define GREEN  "\033[38;5;142m"          // Gruvbox Green (#98971a)
#define YELLOW "\033[38;5;214m"          // Gruvbox Yellow (#d79921)
#define BLUE   "\033[38;5;109m"          // Gruvbox Blue (#458588)
#define CYAN   "\033[38;5;108m"          // Gruvbox Aqua/Cyan (#689d6a)
#define WHITE  "\033[38;5;223m"          // Gruvbox FG1 (#ebdbb2)
#define PURPLE "\033[38;5;141m"          // Gruvbox Purple (#b16286) -> For TRACE logs

constexpr const char* ANSI_REGEX    = "\033\\[[0-9;]*m";
constexpr const char* REL_PATH_LOGS = ".cache/wavy/logs";

#define FILENAME \
  (__builtin_strrchr(__FILE__, '/') ? __builtin_strrchr(__FILE__, '/') + 1 : __FILE__)

#define GET_HOME_OR_RETURN(var)                                               \
  do                                                                          \
  {                                                                           \
    var = std::getenv("HOME");                                                \
    if (!(var))                                                               \
    {                                                                         \
      std::cerr << "ERROR: Unable to determine '" << var << "' directory.\n"; \
      return;                                                                 \
    }                                                                         \
  } while (0)

#define LOG_FMT(str) BOLD str RESET

// clang-format off
#define LOG_CATEGORIES                         \
  X(DECODER,         "#DECODER_LOG         ")  \
  X(TRANSCODER,      "#TRANSCODER_LOG      ")  \
  X(LIBAV,           "#LIBAV_LOG           ")  \
  X(AUDIO,           "#AUDIO_LOG           ")  \
  X(NET,             "#NETWORK_LOG         ")  \
  X(FETCH,           "#TSFETCH_LOG         ")  \
  X(PLUGIN,          "#PLUGIN_LOG          ")  \
  X(HLS,             "#HLS_LOG             ")  \
  X(M3U8_PARSER,     "#M3U8_PARSER_LOG     ")  \
  X(CMD_LINE_PARSER, "#CMD_LINE_PARSER_LOG ")  \
  X(UNIX,            "#UNIX_LOG            ")  \
  X(DISPATCH,        "#DISPATCH_LOG        ")  \
  X(SERVER,          "#SERVER_LOG          ")  \
  X(SERVER_DWNLD,    "#SERVER_DWNLD_LOG    ")  \
  X(SERVER_UPLD,     "#SERVER_UPLD_LOG     ")  \
  X(SERVER_EXTRACT,  "#SERVER_EXTRACT_LOG  ")  \
  X(SERVER_VALIDATE, "#SERVER_VALIDATE_LOG ")  \
  X(OWNER,           "#OWNER_LOG           ")  \
  X(CLIENT,          "#CLIENT_LOG          ")  \
  X(FLAC,            "#FLAC_LOG            ")  \
  X(NONE,            "")
// clang-format on

// Generate string constants
#define X(name, str) constexpr const char* name##_LOG = LOG_FMT(str);
LOG_CATEGORIES
#undef X

namespace libwavy::log
{

// In priority order
enum SeverityLevel
{
  __ERROR__,
  __WARNING__,
  __INFO__,
  __TRACE__,
  __DEBUG__
};

template <typename T> constexpr auto log_prefix() -> const char*;

#define X(name, str)       \
  struct name##_Tag        \
  {                        \
  };                       \
  using name = name##_Tag; \
  template <> constexpr const char* log_prefix<name##_Tag>() { return LOG_FMT(str); }
LOG_CATEGORIES
#undef X
#undef LOG_FMT

/* --------------------------------------------------
 *                  WAVY LOGGER API
 * --------------------------------------------------*/
WAVY_API void set_log_level(SeverityLevel level);
WAVY_API void init_logging();
WAVY_API auto strip_ansi(const std::string& input) -> std::string;
WAVY_API auto get_current_timestamp() -> std::string;

inline WAVY_API void flush_logs() { boost::log::core::get()->flush(); }

inline static const std::map<SeverityLevel, boost::log::trivial::severity_level>
  LOG_LEVEL_ENUM_MAP = {
    {__ERROR__, boost::log::trivial::error}, {__WARNING__, boost::log::trivial::warning},
    {__INFO__, boost::log::trivial::info},   {__TRACE__, boost::log::trivial::trace},
    {__DEBUG__, boost::log::trivial::debug},
};

inline static const std::map<std::string, SeverityLevel> LOG_LEVEL_STR_MAP = {{"ERROR", __ERROR__},
                                                                              {"WARN", __WARNING__},
                                                                              {"INFO", __INFO__},
                                                                              {"TRACE", __TRACE__},
                                                                              {"DEBUG", __DEBUG__}};

/***************************************************
 *              !!! NOTE !!!
 *
 * It is NOT RECOMMENDED to use these logging macros
 * on your own!! Use libwavy/log-macros.hpp which is
 * a much better container and is easy to call.
 *
 ***************************************************/

#ifdef WAVY__INTERNAL_LOGGING_IMPL

// Macros for logging
#define THREAD_ID    BOLD << "[Worker " << boost::this_thread::get_id() << "] " << RESET
#define _TRACE_BACK_ "[" << FILENAME << ":" << __LINE__ << " - " << __func__ << "] "

#define LOG_TRACE   BOOST_LOG_TRIVIAL(trace) << _TRACE_BACK_
#define LOG_INFO    BOOST_LOG_TRIVIAL(info)
#define LOG_WARNING BOOST_LOG_TRIVIAL(warning)
#define LOG_ERROR   BOOST_LOG_TRIVIAL(error) << _TRACE_BACK_
#define LOG_DEBUG   BOOST_LOG_TRIVIAL(debug)

// Async logging macros (include thread ID)
#define LOG_TRACE_ASYNC   BOOST_LOG_TRIVIAL(trace) << THREAD_ID << _TRACE_BACK_
#define LOG_INFO_ASYNC    BOOST_LOG_TRIVIAL(info) << THREAD_ID
#define LOG_WARNING_ASYNC BOOST_LOG_TRIVIAL(warning) << THREAD_ID
#define LOG_ERROR_ASYNC   BOOST_LOG_TRIVIAL(error) << THREAD_ID << _TRACE_BACK_
#define LOG_DEBUG_ASYNC   BOOST_LOG_TRIVIAL(debug) << THREAD_ID

#endif

} // namespace libwavy::log
