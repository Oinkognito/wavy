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

#include <cstring>
#include <libwavy/logger.hpp>
#include <map>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>

namespace libwavy::utils::cmdline
{
class CmdLineParser
{
public:
  CmdLineParser(std::span<char* const> argv, std::string usage) : usage_text_(std::move(usage))
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
          arguments_[key]   = value;
        }
        else
        {
          std::string key = arg.substr(2);
          arguments_[key] = "";
        }
      }
      else
      {
        // Handle any invalid arguments
        LOG_ERROR << "Invalid argument format: " << arg;
        throw std::invalid_argument("Invalid argument format: " + arg);
      }
    }
  }

  void requireMinArgs(size_t min_argc, size_t actual_argc) const
  {
    if (actual_argc < min_argc)
    {
      LOG_ERROR << "Not enough arguments provided. Expected at least " << min_argc << ", but got "
                << actual_argc << ".";
      print_usage_and_exit();
    }
  }

  [[nodiscard]] auto get(const std::string& key, const std::string& default_value = "") const
    -> std::string
  {
    auto it = arguments_.find(key);
    return (it != arguments_.end()) ? it->second : default_value;
  }

  [[nodiscard]] auto get_int(const std::string& key, int default_value = -1) const -> int
  {
    auto it = arguments_.find(key);
    if (it != arguments_.end())
    {
      try
      {
        return std::stoi(it->second);
      }
      catch (const std::invalid_argument& e)
      {
        // Return the default value in case of invalid integer conversion
        LOG_ERROR << "Invalid integer argument for key '" << key << "': " << it->second;
        LOG_WARNING << "Default value " << default_value << " being passed to key: '" << key
                    << "'.";
        return default_value;
      }
      catch (const std::out_of_range& e)
      {
        // Return the default value in case of out-of-range error
        LOG_ERROR << "Integer argument for key '" << key << "' is out of range: " << it->second;
        LOG_WARNING << "Default value " << default_value << " being passed to key: '" << key
                    << "'.";
        return default_value;
      }
    }
    return default_value;
  }

  [[nodiscard]] auto has(const std::string& key) const -> bool
  {
    return arguments_.find(key) != arguments_.end();
  }

  void print_usage_and_exit() const
  {
    std::cerr << "Usage:\n" << usage_text_ << "\n";
    std::exit(0);
  }

private:
  std::map<std::string, std::string> arguments_;
  std::string                        usage_text_;
};
} // namespace libwavy::utils::cmdline
