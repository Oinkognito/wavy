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
  StorageOwnerID m_nickname;
  IPAddr         m_server;
  RelPath        m_pluginPath;
  GlobalState    m_globalState;
  int            m_bitrate;
  RelPath        m_audioBackendLibPath;

public:
  WavyClient(StorageOwnerID nickname, IPAddr server, RelPath plugin_path, const int bitrate,
             const RelPath& audioBackendLibPath)
      : m_nickname(std::move(nickname)), m_server(std::move(server)),
        m_pluginPath(std::move(plugin_path)), m_bitrate(bitrate),
        m_audioBackendLibPath(std::move(audioBackendLibPath))
  {
  }

  auto start(bool flac_found, int index) -> int
  {
    LOG_INFO << "Starting Client Fetch and Playback Daemon...";

    fetch::SegmentFetcherPtr fetcher;

    try
    {
      // Attempt to load the plugin dynamically based on the path
      fetcher = libwavy::fetch::plugin::FetcherFactory::create(m_pluginPath, m_server);
    }
    catch (const std::exception& e)
    {
      LOG_ERROR << PLUGIN_LOG << "Plugin error: " << e.what();
      return WAVY_RET_FAIL;
    }

    // Fetch client list and audio ID
    const std::vector<std::string> clients = fetcher->fetch_client_list(m_server, m_nickname);
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
    if (!fetcher->fetchAndPlay(m_nickname, audio_id, m_globalState, m_bitrate, flac_found,
                               m_audioBackendLibPath))
    {
      LOG_ERROR << RECEIVER_LOG << "Something went horribly wrong while fetching!!";
      return WAVY_RET_FAIL;
    }

    return WAVY_RET_SUC;
  }
};
} // namespace libwavy::components::client
