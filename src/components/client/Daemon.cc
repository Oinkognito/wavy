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

#include <libwavy/components/client/daemon.hpp>

namespace libwavy::components::client
{

auto WavyClient::start(bool flac_found, int index) -> int
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

} // namespace libwavy::components::client
