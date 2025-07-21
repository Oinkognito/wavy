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

#include <cstdlib>
#include <libwavy/common/types.hpp>
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
  INIT_WAVY_LOGGER();

  if (argc < 3)
  {
    lwlog::ERROR<_>("{} <server-ip> <path>", argv[0]);
    return EXIT_FAILURE;
  }
  const IPAddr     server_ip = argv[1];
  const NetTarget  route     = argv[2];
  asio::io_context ioc;
  ssl::context     ctx(ssl::context::tlsv12_client);
  ctx.set_verify_mode(ssl::verify_none);
  libwavy::network::HttpsClient client(ioc, ctx, server_ip);

  try
  {
    NetResponse result = client.get(route);

    if (result.empty())
      lwlog::WARN<_>("Found nothing.");
    else
      // \n to make result look better
      lwlog::INFO<_>("Received: \n{}", result);
  }
  catch (std::exception& e)
  {
    lwlog::ERROR<_>("Failed: {}", e.what());
  }

  return EXIT_SUCCESS;
}
