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

#include "ABRManager.hpp"
#include <boost/asio.hpp>

auto main(int argc, char* argv[]) -> int
{
  if (argc != 2)
  {
    std::cerr << "Usage: " << argv[0] << " <network-stream>" << std::endl;
    return EXIT_FAILURE;
  }
  try
  {
    boost::asio::io_context ioc;

    // Replace with your HLS master playlist URL
    std::string master_url = argv[1];

    libwavy::abr::ABRManager abr_manager(ioc, master_url);
    abr_manager.selectBestBitrate();
  }
  catch (const std::exception& e)
  {
    std::cerr << "[ERROR] Exception: " << e.what() << "\n";
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
