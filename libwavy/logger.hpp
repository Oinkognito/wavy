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
#include <iomanip>
#include <iostream>
#include <sstream>

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

#define FILENAME \
  (__builtin_strrchr(__FILE__, '/') ? __builtin_strrchr(__FILE__, '/') + 1 : __FILE__)

// Define log categories
#define LOG_CATEGORIES                                   \
  X(DECODER, BOLD "#DECODER_LOG " RESET)                 \
  X(ENCODER, BOLD "#ENCODER_LOG " RESET)                 \
  X(TRANSCODER, BOLD "#TRANSCODER_LOG" RESET)            \
  X(LIBAV, BOLD "#LIBAV_LOG" RESET)                      \
  X(NET, BOLD "#NETWORK_LOG" RESET)                      \
  X(HLS, BOLD "#HLS_LOG" RESET)                          \
  X(UNIX, BOLD "#UNIX_LOG" RESET)                        \
  X(DISPATCH, BOLD "#DISPATCH_LOG " RESET)               \
  X(SERVER, BOLD "#SERVER_LOG " RESET)                   \
  X(SERVER_DWNLD, BOLD "#SERVER_DWNLD_LOG " RESET)       \
  X(SERVER_UPLD, BOLD "#SERVER_UPLD_LOG " RESET)         \
  X(SERVER_EXTRACT, BOLD "#SERVER_EXTRACT_LOG " RESET)   \
  X(SERVER_VALIDATE, BOLD "#SERVER_VALIDATE_LOG " RESET) \
  X(RECEIVER, BOLD "#RECEIVER_LOG " RESET)

// Generate string constants
#define X(name, str) constexpr const char* name##_LOG = str;
LOG_CATEGORIES
#undef X

namespace logger
{

enum SeverityLevel
{
  TRACE,
  INFO,
  WARNING,
  ERROR,
  DEBUG
};

inline auto strip_ansi(const std::string& input) -> std::string
{
  static const boost::regex ansi_regex("\033\\[[0-9;]*m");
  return boost::regex_replace(input, ansi_regex, "");
}

inline auto get_current_timestamp() -> std::string
{
  using namespace std::chrono;

  auto now      = system_clock::now();
  auto now_time = system_clock::to_time_t(now);
  auto now_ms   = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;

  std::ostringstream oss;
  oss << std::put_time(std::localtime(&now_time), "%H:%M:%S") << "." << std::setw(3)
      << std::setfill('0') << now_ms.count();
  return oss.str();
}

void init_logging()
{
  namespace bfs      = boost::filesystem;
  namespace sinks    = boost::log::sinks;
  namespace expr     = boost::log::expressions;
  namespace keywords = boost::log::keywords;

  const char* home = getenv("HOME");
  if (!home)
  {
    std::cerr << "ERROR: Unable to determine HOME directory for logging." << std::endl;
    return;
  }

  bfs::path log_dir = bfs::path(home) / ".cache/wavy/logs";

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

  boost::log::add_console_log(
    std::cout,
    keywords::format =
      expr::stream << BOLD << "[" << get_current_timestamp() << "] "
                   << expr::if_(expr::attr<boost::log::trivial::severity_level>("Severity") ==
                                boost::log::trivial::trace)[expr::stream << PURPLE << "[TRACE]   "]
                   << expr::if_(expr::attr<boost::log::trivial::severity_level>("Severity") ==
                                boost::log::trivial::info)[expr::stream << GREEN << "[INFO]    "]
                   << expr::if_(
                        expr::attr<boost::log::trivial::severity_level>("Severity") ==
                        boost::log::trivial::warning)[expr::stream << YELLOW << "[WARNING] "]
                   << expr::if_(expr::attr<boost::log::trivial::severity_level>("Severity") ==
                                boost::log::trivial::error)[expr::stream << RED << "[ERROR]   "]
                   << expr::if_(expr::attr<boost::log::trivial::severity_level>("Severity") ==
                                boost::log::trivial::debug)[expr::stream << BLUE << "[DEBUG]   "]
                   << RESET << expr::smessage);

  // File logging (without ANSI codes)
  using text_sink = sinks::synchronous_sink<sinks::text_file_backend>;
  boost::shared_ptr<text_sink> file_sink =
    boost::make_shared<text_sink>(keywords::file_name     = log_file,
                                  keywords::rotation_size = 10 * 1024 * 1024, // 10 MB
                                  keywords::auto_flush    = true);

  // Custom Filtering: Strip ANSI codes before writing logs to a file
  file_sink->set_formatter(
    [](boost::log::record_view const& rec, boost::log::formatting_ostream& strm)
    {
      auto        severity    = rec[boost::log::trivial::severity];
      auto        message_ref = rec[expr::smessage];
      std::string message     = message_ref ? message_ref.get() : "";

      // Strip ANSI escape codes
      static const boost::regex ansi_regex("\033\\[[0-9;]*m");
      message = boost::regex_replace(message, ansi_regex, "");

      strm << "[" << get_current_timestamp() << "] "
           << (severity ? severity.get() : boost::log::trivial::info) << " " << message;
    });

  boost::log::core::get()->add_sink(file_sink);

  boost::log::add_common_attributes();
}

inline void flush_logs() { boost::log::core::get()->flush(); }

// Macros for logging
#define THREAD_ID    BOLD << "[Worker " << boost::this_thread::get_id() << "] " << RESET
#define _TRACE_BACK_ "[" << FILENAME << ":" << __LINE__ << " - " << __func__ << "] "

#define LOG_TRACE   BOOST_LOG_TRIVIAL(trace) << _TRACE_BACK_
#define LOG_INFO    BOOST_LOG_TRIVIAL(info)
#define LOG_WARNING BOOST_LOG_TRIVIAL(warning)
#define LOG_ERROR   BOOST_LOG_TRIVIAL(error) << _TRACE_BACK_
#define LOG_DEBUG   BOOST_LOG_TRIVIAL(debug) << _TRACE_BACK_

// Async logging macros (include thread ID)
#define LOG_TRACE_ASYNC   BOOST_LOG_TRIVIAL(trace) << THREAD_ID << _TRACE_BACK_
#define LOG_INFO_ASYNC    BOOST_LOG_TRIVIAL(info) << THREAD_ID
#define LOG_WARNING_ASYNC BOOST_LOG_TRIVIAL(warning) << THREAD_ID
#define LOG_ERROR_ASYNC   BOOST_LOG_TRIVIAL(error) << THREAD_ID << _TRACE_BACK_
#define LOG_DEBUG_ASYNC   BOOST_LOG_TRIVIAL(debug) << THREAD_ID << _TRACE_BACK_

} // namespace logger
