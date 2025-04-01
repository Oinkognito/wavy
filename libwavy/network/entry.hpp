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

#include <boost/asio.hpp>
#include <boost/asio/ssl/error.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>

#include <libwavy/common/macros.hpp>
#include <libwavy/logger.hpp>

namespace ssl   = boost::asio::ssl;
namespace beast = boost::beast;
namespace http  = beast::http;
namespace asio  = boost::asio;
using tcp       = asio::ip::tcp;

namespace libwavy::network
{

class HttpsClient
{
public:
  HttpsClient(asio::io_context& ioc, ssl::context& ctx, std::string server)
      : resolver_(ioc), stream_(ioc, ctx), server_(std::move(server))
  {
  }

  /**
   * @brief Perform a GET request to the specified target.
   * @param target The HTTP target (e.g., "/hls/somefile.m3u8").
   * @return The response body as a string, or an empty string on failure.
   */
  auto get(const std::string& target) -> std::string
  {
    try
    {
      // Resolve the server
      auto const results = resolver_.resolve(server_, WAVY_SERVER_PORT_NO_STR);

      // Establish TCP connection
      asio::connect(stream_.next_layer(), results.begin(), results.end());

      // Perform SSL handshake
      stream_.handshake(ssl::stream_base::client);

      // Create and send the HTTP request
      http::request<http::string_body> req{http::verb::get, target, 11};
      req.set(http::field::host, server_);
      req.set(http::field::user_agent, "WavyClient");
      http::write(stream_, req);

      // Read the response
      beast::flat_buffer                 buffer;
      http::response<http::dynamic_body> res;
      http::read(stream_, buffer, res);

      // Convert response body to string
      std::string response_data = boost::beast::buffers_to_string(res.body().data());

      // Properly close the stream
      beast::error_code ec;
      stream_.shutdown(ec);
      if (ec == asio::error::eof)
      {
        ec.clear();
      }
      else if (ec)
      {
        LOG_WARNING << RECEIVER_LOG << "Stream shutdown error: " << ec.message();
      }

      return response_data;
    }
    catch (const std::exception& e)
    {
      LOG_ERROR << RECEIVER_LOG << "HTTPS request failed: " << e.what();
      return "";
    }
  }

private:
  tcp::resolver                  resolver_;
  beast::ssl_stream<tcp::socket> stream_;
  std::string                    server_;
};

} // namespace libwavy::network
