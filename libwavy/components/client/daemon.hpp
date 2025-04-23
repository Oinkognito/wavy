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

#include <libwavy/common/types.hpp>
#include <libwavy/tsfetcher/interface.hpp>
#include <libwavy/tsfetcher/plugin/entry.hpp>
#include <libwavy/utils/audio/entry.hpp>
#include <utility>

namespace libwavy::components::client
{
class WavyClient
{
private:
  StorageOwnerID _ip_id;
  IPAddr         _server;
  RelPath        _plugin_path;
  GlobalState    gs;
  int            _bitrate;
  RelPath        _audio_backend_lib_path;

public:
  WavyClient(StorageOwnerID ip_id, IPAddr server, RelPath plugin_path, const int bitrate,
             const RelPath& audioBackendLibPath)
      : _ip_id(std::move(ip_id)), _server(std::move(server)), _plugin_path(std::move(plugin_path)),
        _bitrate(bitrate), _audio_backend_lib_path(std::move(audioBackendLibPath))
  {
  }

  auto start(bool flac_found, int index) -> int
  {
    LOG_INFO << "Starting Client Fetch and Playback Daemon...";

    fetch::SegmentFetcherPtr fetcher;

    try
    {
      // Attempt to load the plugin dynamically based on the path
      fetcher = libwavy::fetch::plugin::FetcherFactory::create(_plugin_path, _ip_id);
    }
    catch (const std::exception& e)
    {
      LOG_ERROR << PLUGIN_LOG << "Plugin error: " << e.what();
      return WAVY_RET_FAIL;
    }

    // Fetch client list and audio ID
    const std::vector<std::string> clients = fetcher->fetch_client_list(_server, _ip_id);
    if (clients.empty())
    {
      LOG_ERROR << "Failed to fetch clients. Exiting...";
      return WAVY_RET_FAIL;
    }

    // Validate the index
    if (index < 0 || index >= static_cast<int>(clients.size()))
    {
      LOG_ERROR << RECEIVER_LOG << "Invalid index. Available range: 0 to " << clients.size() - 1;
      return WAVY_RET_FAIL;
    }

    StorageAudioID audio_id = clients[index];

    // Fetch the transport stream
    if (!fetcher->fetchAndPlay(_ip_id, audio_id, gs, _bitrate, flac_found, _audio_backend_lib_path))
    {
      LOG_ERROR << RECEIVER_LOG << "Something went horribly wrong while fetching!!";
      return WAVY_RET_FAIL;
    }

    return WAVY_RET_SUC;
  }
};
} // namespace libwavy::components::client
