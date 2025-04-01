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

#include "toml.hpp"
#include <fstream>
#include <string>

namespace libwavy::Toml
{

class TomlGenerator
{
public:
  TomlGenerator() = default;

  void addValueStr(const std::string& key, const std::string& value)
  {
    data.insert_or_assign(key, toml::value<std::string>{value});
  }

  void addValueInt(const std::string& key, int value)
  {
    data.insert_or_assign(key, toml::value<int64_t>{value});
  }

  void addValueDbl(const std::string& key, double value)
  {
    data.insert_or_assign(key, toml::value<double>{value});
  }

  void addValueBool(const std::string& key, bool value)
  {
    data.insert_or_assign(key, toml::value<bool>{value});
  }

  // Create a table (not nested)
  void createTable(const std::string& tableName)
  {
    if (!data.contains(tableName))
    {
      data.insert_or_assign(tableName, toml::table{});
    }
  }

  // Add a key-value pair inside a non-nested table
  template <typename T>
  void addTableValue(const std::string& table, const std::string& key, T value)
  {
    if (!data.contains(table))
    {
      createTable(table);
    }
    data[table].as_table()->insert_or_assign(key, value);
  }

  template <typename T>
  void addTableArray(const std::string& table, const std::string& key, const std::vector<T>& values)
  {
    if (!data.contains(table))
    {
      createTable(table);
    }
    toml::array arr;
    for (const auto& value : values)
    {
      arr.push_back(value);
    }
    data[table].as_table()->insert_or_assign(key, arr);
  }

  // Save to file
  void saveToFile(const std::string& filename)
  {
    std::ofstream file(filename);
    if (file.is_open())
    {
      file << data;
      file.close();
    }
  }

private:
  toml::table data;
};

} // namespace libwavy::Toml
