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

#include <cstring>
#include <libwavy/common/api/entry.hpp>
#include <libwavy/log-macros.hpp>
#include <map>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>

namespace libwavy::utils::cmdline
{
class WAVY_API CmdLineParser
{
public:
  CmdLineParser(std::span<char* const> argv, std::string usage) : m_usageText(std::move(usage))
  {
    for (size_t i = 1; i < argv.size(); ++i)
    {
      std::string arg = argv[i];
      if (arg == "--help" || arg == "-h")
      {
        print_usage_and_exit();
      }
      if (arg.starts_with("--"))
      {
        size_t eq_pos = arg.find('=');
        if (eq_pos != std::string::npos)
        {
          std::string key   = arg.substr(2, eq_pos - 2);
          std::string value = arg.substr(eq_pos + 1);
          m_args[key]       = value;
        }
        else
        {
          std::string key = arg.substr(2);
          m_args[key]     = "";
        }
      }
      else
      {
        // Handle any invalid arguments
        log::ERROR<log::CMD_LINE_PARSER>("Invalid argument format: {}", arg);
        throw std::invalid_argument("Invalid argument format: " + arg);
      }
    }
  }

  void requireMinArgs(size_t min_argc, size_t actual_argc) const
  {
    if (actual_argc < min_argc)
    {
      log::ERROR<log::CMD_LINE_PARSER>(
        "Not enough arguments provided. Expected at least {}, but got {}.", min_argc, actual_argc);
      print_usage_and_exit();
    }
  }

  [[nodiscard]] auto get(const std::string& key, const std::string& default_value = "") const
    -> std::string
  {
    auto it = m_args.find(key);
    return (it != m_args.end()) ? it->second : default_value;
  }

  [[nodiscard]] auto get_int(const std::string& key, int default_value = -1) const -> int
  {
    auto it = m_args.find(key);
    if (it != m_args.end())
    {
      try
      {
        return std::stoi(it->second);
      }
      catch (const std::invalid_argument& e)
      {
        // Return the default value in case of invalid integer conversion
        log::ERROR<log::CMD_LINE_PARSER>("Invalid integer argument for key '{}': {}", key,
                                         it->second);
        log::WARN<log::CMD_LINE_PARSER>("Default value {} being passed to key: '{}'.",
                                        default_value, key);
        return default_value;
      }
      catch (const std::out_of_range& e)
      {
        // Return the default value in case of out-of-range error
        log::ERROR<log::CMD_LINE_PARSER>("Integer argument for key '{}' is out of range: {}", key,
                                         it->second);
        log::WARN<log::CMD_LINE_PARSER>("Default value {} being passed to key: '{}'.",
                                        default_value, key);
        return default_value;
      }
    }
    return default_value;
  }

  [[nodiscard]] auto has(const std::string& key) const -> bool
  {
    return m_args.find(key) != m_args.end();
  }

  void print_usage_and_exit() const
  {
    std::cerr << "Usage:\n" << m_usageText << "\n";
    std::exit(0);
  }

private:
  std::map<std::string, std::string> m_args;
  std::string                        m_usageText;
};
} // namespace libwavy::utils::cmdline
