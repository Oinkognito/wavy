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

#include <functional>
#include <libwavy/common/state.hpp>
#include <libwavy/common/types.hpp>
#include <memory>

namespace libwavy::fetch
{

class ISegmentFetcher
{
public:
  virtual ~ISegmentFetcher() = default;

  virtual auto fetchAndPlay(const StorageOwnerID& nickname, const StorageAudioID& audio_id,
                            int desired_bandwidth, bool& flac_found,
                            const RelPath& audio_backend_lib_path) -> bool = 0;

  virtual auto fetchOwnersList(const IPAddr& server, const StorageOwnerID& targetNickname)
    -> Owners = 0;
};

extern "C"
{
  using FetcherCreateFn        = ISegmentFetcher*();
  using FetcherDestroyFn       = void(ISegmentFetcher*);
  using FetcherCreateWithArgFn = ISegmentFetcher*(const char* server_addr);
}

using SegmentFetcherPtr = std::unique_ptr<libwavy::fetch::ISegmentFetcher,
                                          std::function<void(libwavy::fetch::ISegmentFetcher*)>>;

} // namespace libwavy::fetch
