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
