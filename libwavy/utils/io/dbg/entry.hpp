#pragma once

#include <autogen/config.h>
#include <fstream>
#include <libwavy/logger.hpp>
#include <string>
#include <type_traits>
#include <vector>

namespace libwavy::dbg
{

template <typename T> class FileWriter
{
  static_assert(std::is_same_v<T, std::string> || std::is_trivially_copyable_v<T>,
                "FileWriter only supports std::string or trivially copyable types.");

public:
  static auto write(const std::vector<T>& data, const std::string& filename) -> bool
  {
    std::ofstream output_file(filename, std::ios::binary);
    if (!output_file)
    {
      LOG_ERROR << DECODER_LOG << "Failed to open output file: " << filename;
      return false;
    }

    if constexpr (std::is_same_v<T, std::string>)
    {
      for (const auto& segment : data)
      {
        output_file.write(segment.data(), segment.size());
      }
      LOG_INFO << DECODER_LOG << "Successfully wrote transport streams to " << filename;
    }
    else
    {
      output_file.write(reinterpret_cast<const char*>(data.data()), data.size() * sizeof(T));
      LOG_INFO << DECODER_LOG << "Successfully wrote decoded audio stream to " << filename;
    }

    output_file.close();
    return true;
  }
};

} // namespace libwavy::dbg
