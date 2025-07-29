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

struct CmdArg
{
  std::vector<std::string> keys;
  std::string              description;

  // For flag args
  CmdArg(std::initializer_list<std::string> k, std::string desc)
      : keys(k), description(std::move(desc))
  {
  }
};

class WAVY_API CmdLineParser
{
public:
  CmdLineParser(std::span<char* const> argv)
  {
    for (size_t i = 1; i < argv.size(); ++i)
    {
      std::string arg = argv[i];
      if (arg == "--help" || arg == "-h")
      {
        m_args["help"] = "true";
        continue;
      }

      if (arg.starts_with("--"))
      {
        size_t eq_pos = arg.find('=');
        if (eq_pos != std::string::npos)
        {
          auto key               = arg.substr(2, eq_pos - 2);
          auto val               = arg.substr(eq_pos + 1);
          m_args[std::move(key)] = std::move(val);
        }
        else
        {
          m_args[arg.substr(2)] = "true"; // boolean flag
        }
      }
      else
      {
        std::cerr << "Invalid argument: " << arg << '\n';
        throw std::invalid_argument("Invalid argument format: " + arg);
      }
    }
  }

  void register_arg(const CmdArg& arg) { m_registeredArgs.push_back(arg); }

  void register_args(std::initializer_list<CmdArg> args)
  {
    for (const auto& a : args)
      register_arg(a);
  }

  template <typename T> auto get(const std::string& key) const -> std::optional<T>
  {
    m_accessedKeys.insert(key);
    auto it = m_args.find(key);
    if (it == m_args.end())
      return std::nullopt;
    return parse_value<T>(it->second);
  }

  template <typename T> auto get_or(const std::string& key, T fallback) const -> T
  {
    m_accessedKeys.insert(key);
    auto val = get<T>(key);
    return val.value_or(fallback);
  }

  [[nodiscard]] auto has(const std::string& key) const -> bool
  {
    m_accessedKeys.insert(key);
    return m_args.find(key) != m_args.end();
  }

  [[nodiscard]] auto get_bool(const std::string& key, bool default_value = false) const -> bool
  {
    m_accessedKeys.insert(key);
    auto it = m_args.find(key);
    if (it != m_args.end())
    {
      std::string val = it->second;
      std::ranges::transform(val.begin(), val.end(), val.begin(), ::tolower);
      return val == "true" || val == "1" || val == "yes";
    }
    return default_value;
  }

  // Get with alias list
  template <typename T> auto get(std::initializer_list<std::string> keys) const -> std::optional<T>
  {
    for (const auto& key : keys)
    {
      m_accessedKeys.insert(key);
      auto it = m_args.find(key);
      if (it != m_args.end())
        return parse_value<T>(it->second);
    }
    return std::nullopt;
  }

  // Get with fallback and alias list
  template <typename T> auto get_or(std::initializer_list<std::string> keys, T fallback) const -> T
  {
    auto val = get<T>(keys);
    return val.value_or(fallback);
  }

  // Has with alias list
  [[nodiscard]] auto has(std::initializer_list<std::string> keys) const -> bool
  {
    for (const auto& key : keys)
    {
      m_accessedKeys.insert(key);
      if (m_args.contains(key))
        return true;
    }
    return false;
  }

  // get_bool with alias list
  [[nodiscard]] auto get_bool(std::initializer_list<std::string> keys,
                              bool default_value = false) const -> bool
  {
    for (const auto& key : keys)
    {
      m_accessedKeys.insert(key);
      auto it = m_args.find(key);
      if (it != m_args.end())
      {
        std::string val = it->second;
        std::ranges::transform(val.begin(), val.end(), val.begin(), ::tolower);
        return val == "true" || val == "1" || val == "yes";
      }
    }
    return default_value;
  }

  void requireMinArgs(size_t min_argc, size_t actual_argc) const
  {
    if (actual_argc < min_argc)
    {
      std::cerr << "Not enough arguments. Expected at least " << min_argc << ", but got "
                << actual_argc << ".\n";
      print_usage();
      throw std::invalid_argument("Too few arguments.");
    }
  }

  void warn_unknown_args(bool exit_on_error = false) const
  {
    bool found_errors = false;
    for (const auto& [key, val] : m_args)
    {
      if (!m_accessedKeys.contains(key))
      {
        std::cerr << "[CLI] Unrecognized or unused CLI argument: --" << key
                  << (val != "true" ? ("=" + val) : "") << "\n";
        found_errors = true;
      }
    }

    if (found_errors && exit_on_error)
      exit(255);
  }

  void print_usage() const
  {
    std::cerr << "Usage:\n";
    for (const auto& arg : m_registeredArgs)
    {
      std::string aliases;
      for (const auto& k : arg.keys)
      {
        aliases += "--" + k + ", ";
      }
      if (!aliases.empty())
        aliases.erase(aliases.size() - 2); // Remove trailing comma+space

      std::cerr << "  " << aliases;
      std::cerr << "\n      " << arg.description << "\n";
    }
  }

private:
  std::map<std::string, std::string> m_args;
  mutable std::set<std::string>      m_accessedKeys;
  std::vector<CmdArg>                m_registeredArgs;

  template <typename T> static auto parse_value(const std::string& s) -> std::optional<T>
  {
    if constexpr (std::is_same_v<T, std::string>)
    {
      return s;
    }
    else if constexpr (std::is_same_v<T, bool>)
    {
      return s == "true" || s == "1";
    }
    else if constexpr (std::is_integral_v<T>)
    {
      T out;
      auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), out);
      if (ec == std::errc())
        return out;
      return std::nullopt;
    }
    else
    {
      std::istringstream iss(s);
      T                  out;
      iss >> out;
      if (!iss.fail())
        return out;
      return std::nullopt;
    }
  }
};
} // namespace libwavy::utils::cmdline
