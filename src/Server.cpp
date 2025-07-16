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

#if __cplusplus < 202002L
#error "Wavy-Server requires C++20 or later."
#endif

#include <libwavy/server/server.hpp>

namespace asio = boost::asio;
namespace ssl  = asio::ssl;

auto main() -> int
{
  try
  {
    libwavy::log::init_logging();
    libwavy::log::set_log_level(libwavy::log::TRACE);
    asio::io_context io_context;
    ssl::context     ssl_context(ssl::context::sslv23);

    ssl_context.set_options(ssl::context::default_workarounds | ssl::context::no_sslv2 |
                            ssl::context::no_sslv3 | ssl::context::single_dh_use);

    ssl_context.use_certificate_file(macros::to_string(macros::SERVER_CERT), ssl::context::pem);
    LOG_TRACE << "Loaded server certificate: " << macros::SERVER_CERT;
    ssl_context.use_private_key_file(macros::to_string(macros::SERVER_PRIVATE_KEY),
                                     ssl::context::pem);
    LOG_TRACE << "Loaded private key file: " << macros::SERVER_PRIVATE_KEY;

    libwavy::server::WavyServer server(io_context, ssl_context, WAVY_SERVER_PORT_NO);
    io_context.run();
  }
  catch (std::exception& e)
  {
    LOG_ERROR << SERVER_LOG << "Exception: " << e.what();
  }

  return 0;
}
