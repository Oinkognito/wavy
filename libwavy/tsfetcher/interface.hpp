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


#include <libwavy/common/state.hpp>
#include <string>

namespace libwavy::fetch
{

class ISegmentFetcher
{
public:
  virtual ~ISegmentFetcher() = default;

  virtual auto fetch(const std::string& ip_id, const std::string& audio_id, GlobalState& gs,
                     int desired_bandwidth, bool& flac_found) -> bool = 0;

  virtual auto fetch_client_list(const std::string& server, const std::string& target_ip_id)
    -> std::vector<std::string> = 0;
};

extern "C"
{
  using FetcherCreateFn        = ISegmentFetcher*();
  using FetcherDestroyFn       = void(ISegmentFetcher*);
  using FetcherCreateWithArgFn = ISegmentFetcher*(const char* server_addr);
}

} // namespace libwavy::fetch
