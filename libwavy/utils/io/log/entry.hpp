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


#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

namespace pluginlog
{

// ANSI colors
inline constexpr const char* RESET   = "\033[0m";
inline constexpr const char* RED     = "\033[31m";
inline constexpr const char* GREEN   = "\033[32m";
inline constexpr const char* YELLOW  = "\033[33m";
inline constexpr const char* CYAN    = "\033[36m";
inline constexpr const char* MAGENTA = "\033[35m";
inline constexpr const char* GRAY    = "\033[90m";

enum class Level
{
  TRACE,
  DEBUG,
  INFO,
  WARN,
  ERROR
};

inline auto level_to_string(Level level) -> const char*
{
  switch (level)
  {
    case Level::TRACE:
      return "TRACE";
    case Level::DEBUG:
      return "DEBUG";
    case Level::INFO:
      return "INFO ";
    case Level::WARN:
      return "WARN ";
    case Level::ERROR:
      return "ERROR";
    default:
      return "LOG  ";
  }
}

inline auto level_to_color(Level level) -> const char*
{
  switch (level)
  {
    case Level::TRACE:
      return GRAY;
    case Level::DEBUG:
      return CYAN;
    case Level::INFO:
      return GREEN;
    case Level::WARN:
      return YELLOW;
    case Level::ERROR:
      return RED;
    default:
      return RESET;
  }
}

inline auto timestamp() -> std::string
{
  using namespace std::chrono;
  auto               now    = system_clock::now();
  auto               time   = system_clock::to_time_t(now);
  auto               millis = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;
  std::tm            tm     = *std::localtime(&time);
  std::ostringstream oss;
  oss << std::put_time(&tm, "%H:%M:%S") << '.' << std::setw(3) << std::setfill('0')
      << millis.count();
  return oss.str();
}

// Logger stream object
class PluginLogStream
{
public:
  PluginLogStream(Level level, std::string tag)
      : _level(level), _tag(std::move(tag)), _stream(),
        _out(level == Level::ERROR ? std::cerr : std::cout)
  {
    _stream << level_to_color(_level) << "[" << timestamp() << "] "
            << "[" << level_to_string(_level) << "] "
            << "[" << _tag << "] ";
  }

  template <typename T> auto operator<<(const T& value) -> PluginLogStream&
  {
    _stream << value;
    return *this;
  }

  ~PluginLogStream()
  {
    _stream << RESET << std::endl;
    _out << _stream.str();
  }

private:
  Level              _level;
  std::string        _tag;
  std::ostringstream _stream;
  std::ostream&      _out;
};

// Macros for convenience
#define PLUGIN_LOG_TRACE(tag) ::pluginlog::PluginLogStream(::pluginlog::Level::TRACE, tag)
#define PLUGIN_LOG_DEBUG(tag) ::pluginlog::PluginLogStream(::pluginlog::Level::DEBUG, tag)
#define PLUGIN_LOG_INFO(tag)  ::pluginlog::PluginLogStream(::pluginlog::Level::INFO, tag)
#define PLUGIN_LOG_WARN(tag)  ::pluginlog::PluginLogStream(::pluginlog::Level::WARN, tag)
#define PLUGIN_LOG_ERROR(tag) ::pluginlog::PluginLogStream(::pluginlog::Level::ERROR, tag)

} // namespace pluginlog
