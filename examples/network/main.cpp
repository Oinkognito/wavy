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

#include <cstdlib>
#include <libwavy/logger.hpp>
#include <libwavy/network/entry.hpp>

/*
 * @GET request to Wavy 
 * 
 * This file demonstrates how to use libwavy::network::HttpsClient
 * class to perform a simple GET request to the Wavy Server using 
 * Boost C++
 *
 * The only params you will need are the server IP and path (route) you want 
 * to get.
 *
 * Note: Wavy has a FIXED port number so giving this is not necessary.
 *
 */

auto main(int argc, char* argv[]) -> int
{
  logger::init_logging();
  if (argc < 3)
  {
    LOG_ERROR << argv[0] << " <server-ip>" << " <path>";
    return EXIT_FAILURE;
  }
  const std::string& server_ip = argv[1];
  const std::string& route     = argv[2];
  asio::io_context   ioc;
  ssl::context       ctx(ssl::context::tlsv12_client);
  ctx.set_verify_mode(ssl::verify_none);
  libwavy::network::HttpsClient client(ioc, ctx, server_ip);

  try
  {
    std::string result = client.get(route);

    if (result.empty())
      LOG_WARNING << "Found nothing.";
    else
      LOG_INFO << "Received: " << std::endl
               << result; // std::endl just to make the result string look better and not deformed
  }
  catch (std::exception& e)
  {
    LOG_ERROR << e.what();
  }

  return EXIT_SUCCESS;
}
