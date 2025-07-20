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

#include <boost/asio.hpp>
#include <libwavy/abrate/ABRManager.hpp>

std::atomic<bool> running{true};

void signalHandler(int signal)
{
  if (signal == SIGINT || signal == SIGTERM)
  {
    running = false;
  }
}

auto main(int argc, char* argv[]) -> int
{
  libwavy::log::init_logging();
  if (argc != 2)
  {
    LOG_ERROR << "Usage: " << argv[0] << " <network-stream>";
    return EXIT_FAILURE;
  }
  try
  {
    boost::asio::io_context ioc;

    // Replace with your HLS master playlist URL
    const std::string master_url = argv[1];

    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    while (running)
    {
      libwavy::abr::ABRManager abr_manager(ioc, master_url);
      abr_manager.selectBestBitrate();
      LOG_INFO << "Waiting for 2 seconds...";
      std::this_thread::sleep_for(std::chrono::seconds(2)); // Poll every 2 seconds
    }
  }
  catch (const std::exception& e)
  {
    LOG_ERROR << "Exception: " << e.what();
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
