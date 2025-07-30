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

  auto start(bool flac_found, int index) -> int;
};
} // namespace libwavy::components::client
