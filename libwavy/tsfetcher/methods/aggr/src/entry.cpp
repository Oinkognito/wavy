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

#include <libwavy/tsfetcher/interface.hpp>
#include <libwavy/tsfetcher/methods/aggr/entry.hpp>

using namespace libwavy::fetch;

extern "C" auto create_fetcher_with_arg(const char* server) -> ISegmentFetcher*
{
  return new TSegFetcher(std::string(server));
}

extern "C" void destroy_fetcher(ISegmentFetcher* ptr) { delete ptr; }
