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

#include <array>
#include <iomanip>
#include <sstream>
#include <string>

namespace libwavy::utils::math
{

inline auto formatSize(double size, const std::string& unit) -> std::string
{
  std::ostringstream oss;
  oss << std::fixed << std::setprecision(2) << size << " " << unit;
  return oss.str();
}

inline auto bytesFormat(size_t bytes) -> std::string
{
  static constexpr std::array<const char*, 4> units    = {"B", "KiB", "MiB", "GiB"};
  static constexpr std::array<size_t, 4>      divisors = {1, 1024, 1024 * 1024, 1024 * 1024 * 1024};

  for (size_t i = 0; i < units.size(); ++i)
  {
    if (bytes < divisors[i + 1])
    {
      return formatSize(static_cast<double>(bytes) / divisors[i], units[i]);
    }
  }

  return formatSize(static_cast<double>(bytes) / divisors[3], units[3]);
}

} // namespace libwavy::utils::math
