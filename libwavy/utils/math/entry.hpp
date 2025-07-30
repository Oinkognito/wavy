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

#include <array>
#include <iomanip>
#include <sstream>
#include <string>

enum ByteTable : std::size_t
{
  ONE_KIB = 1024,
  ONE_MIB = ONE_KIB * ONE_KIB,
  ONE_GIB = ONE_MIB * ONE_KIB
};

namespace libwavy::utils::math
{

inline auto formatSize(double size, const std::string& unit) -> std::string
{
  std::ostringstream oss;
  oss << std::fixed << std::setprecision(2) << size << " " << unit;
  return oss.str();
}

inline auto bytesFormat(std::size_t bytes) -> std::string
{
  static constexpr std::array<const char*, 4> units    = {"B", "KiB", "MiB", "GiB"};
  static constexpr std::array<std::size_t, 4> divisors = {1, ONE_KIB, ONE_MIB, ONE_GIB};

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
