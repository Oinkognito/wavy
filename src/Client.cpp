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
#include <libwavy/logger.hpp>
#include <libwavy/utils/cmd-line/parser.hpp>

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
    libwavy::log::DEBUG); // anything with INFO and above priority will be printed

  std::string usage = std::string(argv[0]) +
                      ": --ipAddr=<ip-id> --index=<index> --serverIP=<server-ip> "
                      "--bitrate-stream=<bitrate-stream> --tsfetchMode=<mode> "
                      "[--tsfetchLib=<so_file>] --audioBackendLibPath=<so_file>";

  libwavy::util::cmdline::CmdLineParser parser(std::span<char* const>(argv, argc), usage);

  const std::string ip_id               = parser.get("ipAddr");
  const int         index               = parser.get_int("index", -1); // Safe parsing of integer
  const std::string server              = parser.get("serverIP");
  const int         bitrate             = parser.get_int("bitrate-stream", 0);
  const std::string audioBackendLibPath = parser.get("audioBackendLibPath");
  const std::string fetch_mode =
    parser.get("fetchMode", "default");                     // Default to "default" if not specified
  const std::string fetch_lib = parser.get("fetchLib", ""); // Default to empty if not specified

  parser.requireMinArgs(5, argc);

  // Check if index or bitrate is valid
  if (index == -1)
  {
    LOG_ERROR << "Invalid or missing index argument.";
    return WAVY_RET_FAIL;
  }

  if (bitrate == 0)
  {
    LOG_ERROR << "Invalid or missing bitrate-stream argument.";
    return WAVY_RET_FAIL;
  }

  std::string plugin_path = "";

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

  bool flac_found = parser.get("--playFlac") == "true" ? true : false;

  libwavy::components::client::WavyClient wavyClient(ip_id, server, plugin_path, bitrate,
                                                     audioBackendLibPath);

  return wavyClient.start(flac_found, index);
}
