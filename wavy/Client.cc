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
  INIT_WAVY_LOGGER_ALL();
  namespace logger = lwlog;
  using Client     = libwavy::log::CLIENT;

  libwavy::utils::cmdline::CmdLineParser parser(std::span<char* const>(argv, argc));

  parser.register_args(
    {{{"nickname", "n"}, "Fetch the desired nickname's songs"},
     {{"index", "idx"}, "The particular index required to be accessed."},
     {{"serverIP", "ip"}, "Wavy server IP"},
     {{"bitrate-stream"},
      "Specify the bitrate stream for playback (will default to max as fallback.)"},
     {{"audioBackendLibPath", "abl"}, "Specify the Audio Backend Shared Library Path."},
     {{"fetchMode"}, "Specify the fetch mode (currently only Aggressive is implemented!)"},
     {{"fetchLib"}, "Specify the fetch mode' shared library"},
     {{"playFlac"}, "Whether to playback as FLAC stream or not. (Boolean flag)"},
     {{"useChunkedStream"},
      "Use chunked streaming (for possibly faster streaming of transport segments.)"}

    });

  const StorageOwnerID nickname = *parser.get<StorageOwnerID>({"nickname", "n"});
  const int            index  = parser.get_or<int>({"index", "idx"}, -1); // Safe parsing of integer
  const IPAddr         server = *parser.get<IPAddr>({"serverIP", "ip"});
  const int            bitrate             = parser.get_or("bitrate-stream", 0);
  const RelPath        audioBackendLibPath = *parser.get<RelPath>(
    {"audioBackendLibPath", "abl"}); // relative to WAVY_FETCHER_PLUGIN_OUTPUT_PATH
  const auto fetch_mode =
    parser.get_or<std::string>("fetchMode", "Aggressive"); // Default to "default" if not specified
  const RelPath fetch_lib = *parser.get<RelPath>("fetchLib"); // Default to empty if not specified

  const bool flac_found         = parser.get_bool("playFlac");
  const bool use_chunked_stream = parser.get_bool("useChunkedStream");

  parser.requireMinArgs(6, argc);

  // Check if index or bitrate is valid
  if (index == -1)
  {
    logger::ERROR<Client>("Invalid or missing index argument.");
    return WAVY_RET_FAIL;
  }

  if (bitrate == 0)
  {
    logger::ERROR<Client>("Invalid or missing bitrate-stream argument.");
    return WAVY_RET_FAIL;
  }

  RelPath plugin_path = "";

  // Check if fetch mode is custom and fetchLib is provided
  if (fetch_mode == "custom")
  {
    if (fetch_lib.empty())
    {
      logger::ERROR<Client>(
        "You must specify --fetchLib=<so file name> when using --fetchMode=custom");
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
    logger::ERROR<Client>("No matching fetcher plugin found for mode: {}!", fetch_mode);
    logger::INFO<Client>("Available fetchers: ");
    for (const auto& gFetcher : gFetchers)
      logger::INFO<Client>("Fetcher: {} ({})", gFetcher.name, gFetcher.plugin_path);
    return WAVY_RET_FAIL;
  }
  else
  {
    logger::INFO<Client>("Proceeding with Fetcher Plugin: {}", plugin_path);
  }

  libwavy::components::client::WavyClient wavyClient(nickname, server, plugin_path, bitrate,
                                                     audioBackendLibPath);

  return wavyClient.start(flac_found, index, use_chunked_stream);
}
