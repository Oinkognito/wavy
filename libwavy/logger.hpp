#pragma once
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
#include <chrono>
#include <format>
#include <iostream>

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

#define GET_HOME_OR_RETURN(var)                                    \
  do                                                               \
  {                                                                \
    var = std::getenv("HOME");                                     \
    if (!(var))                                                    \
    {                                                              \
      std::cerr << "ERROR: Unable to determine HOME directory.\n"; \
      return;                                                      \
    }                                                              \
  } while (0)

#define LOG_FMT(str) BOLD str RESET

#define LOG_CATEGORIES                        \
  X(DECODER, "#DECODER_LOG         ")         \
  X(TRANSCODER, "#TRANSCODER_LOG      ")      \
  X(LIBAV, "#LIBAV_LOG           ")           \
  X(AUDIO, "#AUDIO_LOG           ")           \
  X(NET, "#NETWORK_LOG         ")             \
  X(FETCH, "#TSFETCH_LOG         ")           \
  X(PLUGIN, "#PLUGIN_LOG          ")          \
  X(HLS, "#HLS_LOG             ")             \
  X(M3U8_PARSER, "#M3U8_PARSER_LOG     ")     \
  X(UNIX, "#UNIX_LOG            ")            \
  X(DISPATCH, "#DISPATCH_LOG        ")        \
  X(SERVER, "#SERVER_LOG          ")          \
  X(SERVER_DWNLD, "#SERVER_DWNLD_LOG    ")    \
  X(SERVER_UPLD, "#SERVER_UPLD_LOG     ")     \
  X(SERVER_EXTRACT, "#SERVER_EXTRACT_LOG  ")  \
  X(SERVER_VALIDATE, "#SERVER_VALIDATE_LOG ") \
  X(OWNER, "#OWNER_LOG           ")           \
  X(RECEIVER, "#RECEIVER_LOG        ")

// Generate string constants
#define X(name, str) constexpr const char* name##_LOG = LOG_FMT(str);
LOG_CATEGORIES
#undef X
#undef LOG_FMT

namespace libwavy::log
{

// In priority order
enum SeverityLevel
{
  ERROR,
  WARNING,
  TRACE,
  INFO,
  DEBUG
};

inline auto strip_ansi(const std::string& input) -> std::string
{
  static const boost::regex ansi_regex(ANSI_REGEX);
  return boost::regex_replace(input, ansi_regex, "");
}

inline auto get_current_timestamp() -> std::string
{
  using namespace std::chrono;

  auto now      = system_clock::now();
  auto now_time = floor<seconds>(now); // truncate to seconds
  auto now_ms   = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;

  return std::format("{:%Y-%m-%d %H:%M:%S}.{:03}", now_time, now_ms.count());
}

inline void init_logging()
{
  namespace bfs     = boost::filesystem;
  namespace logging = boost::log;
  namespace trivial = boost::log::trivial;
  namespace sinks   = boost::log::sinks;
  namespace expr    = boost::log::expressions;
  namespace kw      = boost::log::keywords;
  using expr::stream;

  using Severity = trivial::severity_level;

  auto console_formatter = []
  {
    return stream << BOLD << "[" << get_current_timestamp() << "] "
                  << expr::if_(expr::attr<Severity>("Severity") ==
                               trivial::trace)[stream << PURPLE << "[TRACE]   "]
                  << expr::if_(expr::attr<Severity>("Severity") ==
                               trivial::info)[stream << GREEN << "[INFO]    "]
                  << expr::if_(expr::attr<Severity>("Severity") ==
                               trivial::warning)[stream << YELLOW << "[WARN]    "]
                  << expr::if_(expr::attr<Severity>("Severity") ==
                               trivial::error)[stream << RED << "[ERROR]   "]
                  << expr::if_(expr::attr<Severity>("Severity") ==
                               trivial::debug)[stream << BLUE << "[DEBUG]   "]
                  << RESET << expr::smessage;
  };

  const char* home;
  GET_HOME_OR_RETURN(home);

  bfs::path log_dir = bfs::path(home) / REL_PATH_LOGS;

  if (!bfs::exists(log_dir))
  {
    if (!bfs::create_directories(log_dir))
    {
      std::cerr << "ERROR: Failed to create log directory: " << log_dir.string() << std::endl;
      return;
    }
  }

  // Define log file path
  std::string log_file = (log_dir / "wavy_%Y-%m-%d_%H-%M-%S.log").string();

  logging::add_console_log(std::cout, kw::format = console_formatter());

  // File logging (without ANSI codes)
  using text_sink = sinks::synchronous_sink<sinks::text_file_backend>;
  boost::shared_ptr<text_sink> file_sink =
    boost::make_shared<text_sink>(kw::file_name     = log_file,
                                  kw::rotation_size = 10 * 1024 * 1024, // 10 MB
                                  kw::auto_flush    = true);

  // Custom Filtering: Strip ANSI codes before writing logs to a file
  file_sink->set_formatter(
    [](boost::log::record_view const& rec, boost::log::formatting_ostream& strm)
    {
      auto        severity    = rec[trivial::severity];
      auto        message_ref = rec[expr::smessage];
      std::string message     = message_ref ? message_ref.get() : "";

      // Strip ANSI escape codes
      static const boost::regex ansi_regex(ANSI_REGEX);
      message = boost::regex_replace(message, ansi_regex, "");

      strm << "[" << get_current_timestamp() << "] " << (severity ? severity.get() : trivial::info)
           << " " << message;
    });

  boost::log::core::get()->add_sink(file_sink);

  boost::log::add_common_attributes();
}

inline void flush_logs() { boost::log::core::get()->flush(); }

inline void set_log_level(SeverityLevel level)
{
  namespace trivial = boost::log::trivial;

  static const std::map<SeverityLevel, trivial::severity_level> level_map = {
    {ERROR, trivial::error}, {WARNING, trivial::warning}, {TRACE, trivial::trace},
    {INFO, trivial::info},   {DEBUG, trivial::debug},
  };

  auto it = level_map.find(level);
  if (it != level_map.end())
  {
    boost::log::core::get()->set_filter(trivial::severity >= it->second);
  }
  else
  {
    std::cerr << "Unknown log level specified.\n";
  }
}

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

} // namespace libwavy::log
