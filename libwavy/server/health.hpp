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

#include <filesystem>
#include <libwavy/common/macros.hpp>
#include <libwavy/common/types.hpp>
#include <string>
#include <unordered_map>

namespace fs = std::filesystem;

namespace libwavy::server
{

class HealthChecker
{
public:
  struct HealthStatus
  {
    bool                                         is_healthy     = true;
    std::string                                  status_message = "OK";
    std::unordered_map<std::string, std::string> checks;
  };

  static auto check_system_health() -> HealthStatus
  {
    HealthStatus status;

    // Check storage directory
    AbsPath storage_path = macros::to_string(macros::SERVER_STORAGE_DIR);
    if (!fs::exists(storage_path) || !fs::is_directory(storage_path))
    {
      status.is_healthy        = false;
      status.checks["storage"] = "FAIL - Directory not accessible";
    }
    else
    {
      status.checks["storage"] = "OK";
    }

    // Check temp directory
    AbsPath temp_path = macros::to_string(macros::SERVER_TEMP_STORAGE_DIR);
    try
    {
      fs::create_directories(temp_path);
      status.checks["temp_storage"] = "OK";
    }
    catch (const std::exception& e)
    {
      status.is_healthy             = false;
      status.checks["temp_storage"] = "FAIL - " + std::string(e.what());
    }

    // Check disk space (simplified)
    try
    {
      auto   space   = fs::space(storage_path);
      double free_gb = static_cast<double>(space.free) / (1024 * 1024 * 1024);
      if (free_gb < 1.0)
      { // Less than 1GB free
        status.is_healthy           = false;
        status.checks["disk_space"] = "WARN - Low disk space: " + std::to_string(free_gb) + "GB";
      }
      else
      {
        status.checks["disk_space"] = "OK - " + std::to_string(free_gb) + "GB free";
      }
    }
    catch (const std::exception& e)
    {
      status.checks["disk_space"] = "UNKNOWN - " + std::string(e.what());
    }

    // Update overall status message
    if (!status.is_healthy)
    {
      status.status_message = "UNHEALTHY";
    }

    return status;
  }
};

} // namespace libwavy::server
