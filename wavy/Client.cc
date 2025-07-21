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

#if __cplusplus < 202002L
#error "Wavy-Client requires C++20 or later."
#endif

#include <autogen/fetcherConfig.h>
#include <iostream>
#include <libwavy/components/client/daemon.hpp>
#include <libwavy/log-macros.hpp>
#include <libwavy/utils/cmd-line/parser.hpp>

using Client = libwavy::log::CLIENT;
namespace lwlog = libwavy::log;

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
  lwlog::init_logging();
  lwlog::set_log_level(lwlog::__DEBUG__); // anything with INFO and above priority will be printed

  std::string usage = std::string(argv[0]) +
                      ": --nickname=<owner-nickname> --index=<index> --serverIP=<server-ip> "
                      "--bitrate-stream=<bitrate-stream> --tsfetchMode=<mode> "
                      "[--tsfetchLib=<so_file>] --audioBackendLibPath=<so_file>";

  libwavy::utils::cmdline::CmdLineParser parser(std::span<char* const>(argv, argc), usage);

  const StorageOwnerID nickname = parser.get("nickname");
  const int            index    = parser.get_int("index", -1); // Safe parsing of integer
  const IPAddr         server   = parser.get("serverIP");
  const int            bitrate  = parser.get_int("bitrate-stream", 0);
  const RelPath        audioBackendLibPath =
    parser.get("audioBackendLibPath"); // relative to WAVY_FETCHER_PLUGIN_OUTPUT_PATH
  const std::string fetch_mode =
    parser.get("fetchMode", "default");                 // Default to "default" if not specified
  const RelPath fetch_lib = parser.get("fetchLib", ""); // Default to empty if not specified

  parser.requireMinArgs(5, argc);

  // Check if index or bitrate is valid
  if (index == -1)
  {
    lwlog::ERROR<Client>("Invalid or missing index argument.");
    return WAVY_RET_FAIL;
  }

  if (bitrate == 0)
  {
    lwlog::ERROR<Client>("Invalid or missing bitrate-stream argument.");
    return WAVY_RET_FAIL;
  }

  RelPath plugin_path = "";

  // Check if fetch mode is custom and fetchLib is provided
  if (fetch_mode == "custom")
  {
    if (fetch_lib.empty())
    {
      lwlog::ERROR<Client>("You must specify --fetchLib=<so file name> when using --fetchMode=custom");
      return WAVY_RET_FAIL;
    }

    // Set the plugin path to the custom library
    plugin_path = std::string(WAVY_FETCHER_PLUGIN_OUTPUT_PATH) + "/" + fetch_lib;
  }
  bool foundFetcher = false;
  for (const auto& gFetcher : gFetchers)
  {
    if (fetch_mode == gFetcher.name)
    {
      plugin_path  = std::string(WAVY_FETCHER_PLUGIN_OUTPUT_PATH) + "/" + gFetcher.plugin_path;
      foundFetcher = true;
      break;
    }
  }

  if (!foundFetcher)
  {
    lwlog::ERROR<Client>("No matching fetcher plugin found for mode: {}!", fetch_mode);
    lwlog::INFO<Client>("Available fetchers: ");
    for (const auto& gFetcher : gFetchers)
      lwlog::INFO<Client>("Fetcher: {} ({})", gFetcher.name, gFetcher.plugin_path);
    return WAVY_RET_FAIL;
  }
  else
  {
    lwlog::INFO<Client>("Proceeding with Fetcher Plugin: {}", plugin_path);
  }

  bool flac_found = parser.get("--playFlac") == "true" ? true : false;

  libwavy::components::client::WavyClient wavyClient(nickname, server, plugin_path, bitrate,
                                                     audioBackendLibPath);

  return wavyClient.start(flac_found, index);
}
