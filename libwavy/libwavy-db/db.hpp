#pragma once

#include "../libwavy-common/logger.hpp"
#include <cstring>
#include <fstream>
#include <iostream>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <utility>

// Database Record Structure
struct Record
{
  uint32_t              key;
  std::array<char, 256> value;

  Record() : key(0) { value.fill(0); }

  Record(uint32_t k, const std::string& v) : key(k)
  {
    value.fill(0);
    std::strncpy(value.data(), v.c_str(), value.size() - 1);
  }
};

namespace libwavy::db
{

class MinimalDB
{
private:
  std::unordered_map<uint32_t, long> index; // Key -> File offset
  std::shared_mutex                  db_mutex;
  std::string                        db_file;
  std::string                        log_file;

  void load_index()
  {
    std::ifstream file(db_file, std::ios::binary);
    if (!file)
      return;

    long   offset = 0;
    Record rec;
    while (file.read(reinterpret_cast<char*>(&rec), sizeof(Record)))
    {
      index[rec.key] = offset;
      offset += sizeof(Record);
    }
    file.close();
  }

  void write_log(const std::string& entry)
  {
    std::ofstream log(log_file, std::ios::app);
    log << entry << std::endl;
  }

public:
  MinimalDB(std::string db_filename = "database.db", std::string log_filename = "wal.log")
      : db_file(std::move(db_filename)), log_file(std::move(log_filename))
  {
    load_index();
  }

  void insert(uint32_t key, const std::string& value)
  {
    std::unique_lock lock(db_mutex);
    std::ofstream    file(db_file, std::ios::binary | std::ios::app);
    if (!file)
    {
      LOG_ERROR_ASYNC << "Error: Cannot open database file.";
      return;
    }

    long   offset = file.tellp();
    Record rec(key, value);
    file.write(reinterpret_cast<const char*>(&rec), sizeof(Record));
    file.close();

    index[key] = offset;
    write_log("INSERT " + std::to_string(key) + " " + value);
  }

  auto get(uint32_t key) -> std::string
  {
    std::shared_lock lock(db_mutex);
    if (index.find(key) == index.end())
      return "Not found";

    std::ifstream file(db_file, std::ios::binary);
    if (!file)
      return "Error: Cannot open database";

    file.seekg(index[key]);
    Record rec;
    file.read(reinterpret_cast<char*>(&rec), sizeof(Record));
    file.close();

    return {rec.value.data()};
  }

  void remove(uint32_t key)
  {
    std::unique_lock lock(db_mutex);
    if (index.find(key) == index.end())
      return;

    index.erase(key);
    write_log("DELETE " + std::to_string(key));
  }

  void restore_from_log()
  {
    std::ifstream log(log_file);
    if (!log)
      return;

    std::string action, value;
    uint32_t    key;

    while (log >> action >> key)
    {
      if (action == "INSERT")
      {
        log.ignore();
        std::getline(log, value);
        insert(key, value);
      }
      else if (action == "DELETE")
      {
        remove(key);
      }
    }
  }
};

} // namespace libwavy::db
