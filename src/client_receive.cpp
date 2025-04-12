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
#include <libwavy/ffmpeg/decoder/entry.hpp>
#include <libwavy/logger.hpp>
#include <libwavy/playback.hpp>
#include <libwavy/tsfetcher/plugin/entry.hpp>

auto decodeAndPlay(GlobalState& gs, bool& flac_found) -> bool
{
  if (gs.transport_segments.empty())
  {
    LOG_ERROR << DECODER_LOG << "No transport stream segments provided";
    return false;
  }

  LOG_INFO << "Decoding transport stream segments...";

  libwavy::ffmpeg::MediaDecoder decoder;
  std::vector<unsigned char>    decoded_audio;
  if (!decoder.decode(gs.transport_segments, decoded_audio))
  {
    LOG_ERROR << DECODER_LOG << "Decoding failed";
    return false;
  }

  try
  {
    LOG_INFO << RECEIVER_LOG << "Starting audio playback...";
    libwavy::audio::AudioPlayer player(decoded_audio, flac_found);
    player.play();
  }
  catch (const std::exception& e)
  {
    LOG_ERROR << AUDIO_LOG << "Audio playback error: " << e.what();
    return false;
  }

  return true;
}

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
    plugin_path = std::string(WAVY_PLUGIN_OUTPUT_PATH) + "/" + fetch_lib;
  }
  else if (fetch_mode == "aggr")
  {
    plugin_path = std::string(WAVY_PLUGIN_OUTPUT_PATH) + "/libwavy_aggr_fetch_plugin.so";
  }
  else
  {
    LOG_ERROR << PLUGIN_LOG << "Plugin not found for fetch mode: " << fetch_mode;
    return WAVY_RET_FAIL;
  }

  bool        flac_found = false;
  GlobalState gs;
  std::unique_ptr<libwavy::fetch::ISegmentFetcher,
                  std::function<void(libwavy::fetch::ISegmentFetcher*)>>
    fetcher;

  try
  {
    // Attempt to load the plugin dynamically based on the path
    fetcher = libwavy::fetch::plugin::FetcherFactory::create(plugin_path, ip_id);
  }
  catch (const std::exception& e)
  {
    LOG_ERROR << PLUGIN_LOG << "Plugin error: " << e.what();
    return WAVY_RET_FAIL;
  }

  // Fetch client list and audio ID
  const std::vector<std::string> clients = fetcher->fetch_client_list(server, ip_id);
  if (clients.empty())
  {
    LOG_ERROR << "Failed to fetch clients. Exiting...";
    return WAVY_RET_FAIL;
  }
  std::string audio_id = clients[index];

  // Validate the index
  if (index < 0 || index >= static_cast<int>(clients.size()))
  {
    LOG_ERROR << RECEIVER_LOG << "Invalid index. Available range: 0 to " << clients.size() - 1;
    return WAVY_RET_FAIL;
  }

  // Fetch the transport stream
  if (!fetcher->fetch(ip_id, audio_id, gs, bitrate, flac_found))
  {
    LOG_ERROR << RECEIVER_LOG << "Something went horribly wrong while fetching!!";
    return EXIT_FAILURE;
  }

  // Decode and play the fetched stream
  if (!decodeAndPlay(gs, flac_found))
  {
    return WAVY_RET_FAIL;
  }

  return WAVY_RET_SUC;
}
