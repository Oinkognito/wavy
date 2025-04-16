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

#if __cplusplus < 202002L
#error "Wavy-Client requires C++20 or later."
#endif

#include <iostream>
#include <libwavy/components/client/daemon.hpp>

void print_client_list(const std::vector<std::string>& clients)
{
  if (clients.empty())
  {
    std::cout << "No clients found.\n";
    return;
  }

  std::cout << "Available Clients:\n";
  for (size_t i = 0; i < clients.size(); ++i)
  {
    std::cout << "  [" << i << "] " << clients[i] << "\n";
  }
}

auto main(int argc, char* argv[]) -> int
{
  libwavy::log::init_logging();
  libwavy::log::set_log_level(
    libwavy::log::INFO); // anything with INFO and above priority will be printed

  if (argc < 5)
  {
    LOG_ERROR << "Usage: " << argv[0]
              << " <ip-id> <index> <server-ip> <bitrate-stream> [--fetchMode=<mode>] "
                 "[--fetchLib=<so_file>]";
    return WAVY_RET_FAIL;
  }

  const std::string ip_id   = argv[1];
  const int         index   = std::stoi(argv[2]);
  const std::string server  = argv[3];
  const int         bitrate = std::stoi(argv[4]);

  std::string plugin_path = "";
  std::string fetch_mode  = "default"; // Default mode if not specified
  std::string fetch_lib   = "";        // Custom fetch library path

  // Parse command-line arguments to get fetch mode and plugin path
  for (int i = 5; i < argc; ++i)
  {
    std::string arg = argv[i];
    if (arg.starts_with("--fetchMode="))
    {
      fetch_mode = arg.substr(12); // Extract the mode from the argument
    }
    else if (arg.starts_with("--fetchLib="))
    {
      fetch_lib = arg.substr(11); // Extract the library file name from the argument
    }
    else
    {
      LOG_ERROR << "Unknown argument: " << arg;
      return WAVY_RET_FAIL;
    }
  }

  // Check if fetch mode is custom and fetchLib is provided
  if (fetch_mode == "custom")
  {
    if (fetch_lib.empty())
    {
      LOG_ERROR << "You must specify --fetchLib=<so file name> when using --fetchMode=custom";
      return WAVY_RET_FAIL;
    }

    // Set the plugin path to the custom library
    plugin_path = std::string(WAVY_FETCHER_PLUGIN_OUTPUT_PATH) + "/" + fetch_lib;
  }
  else if (fetch_mode == "aggr")
  {
    plugin_path = std::string(WAVY_FETCHER_PLUGIN_OUTPUT_PATH) + "/libwavy_aggr_fetch_plugin.so";
  }
  else
  {
    LOG_ERROR << PLUGIN_LOG << "Plugin not found for fetch mode: " << fetch_mode;
    return WAVY_RET_FAIL;
  }

  bool flac_found = false;

  libwavy::components::client::WavyClient wavyClient(ip_id, server, plugin_path, bitrate);

  return wavyClient.start(flac_found, index);
}
