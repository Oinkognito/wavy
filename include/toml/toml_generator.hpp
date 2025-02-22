#pragma once

#include "toml.hpp"
#include <fstream>
#include <string>

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
