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

#include <boost/asio.hpp>
#include <boost/asio/ssl/error.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>

#include <libwavy/common/macros.hpp>
#include <libwavy/common/types.hpp>
#include <libwavy/log-macros.hpp>

namespace ssl   = boost::asio::ssl;
namespace beast = boost::beast;
namespace http  = beast::http;
namespace asio  = boost::asio;
using tcp       = asio::ip::tcp;

namespace libwavy::network
{

class WAVY_API HttpsClient
{
public:
  HttpsClient(asio::io_context& ioc, ssl::context& ssl_ctx, IPAddr server)
      : m_ioCtx(ioc), m_sslCtx(ssl_ctx), m_server(std::move(server))
  {
  }

  void setTimeout(std::chrono::seconds timeout) { m_timeout = timeout; }

  void cancelCurrentRequest()
  {
    if (m_socket && beast::get_lowest_layer(*m_socket).is_open())
    {
      beast::get_lowest_layer(*m_socket).cancel();
    }
  }

  auto get(const NetTarget& target) -> NetResponse
  {
    return makeRequest(http::verb::get, target, "");
  }

  auto post(const NetTarget& target, const std::string& body) -> NetResponse
  {
    return makeRequest(http::verb::post, target, body);
  }

  auto streamChunked(const NetTarget&                              target,
                     std::function<void(const char*, std::size_t)> on_chunk) -> bool
  {
    try
    {
      tcp::resolver resolver{m_ioCtx};
      m_socket = std::make_unique<beast::ssl_stream<tcp::socket>>(m_ioCtx, m_sslCtx);

      auto results = resolver.resolve(m_server, WAVY_SERVER_PORT_NO_STR);
      asio::connect(beast::get_lowest_layer(*m_socket), results.begin(), results.end());
      m_socket->handshake(ssl::stream_base::client);

      // Prepare GET request
      http::request<http::empty_body> req{http::verb::get, target, 11};
      req.set(http::field::host, m_server);
      req.set(http::field::user_agent, "WavyClient");

      // Send the request
      http::write(*m_socket, req);

      // Prepare parser for chunked response
      beast::flat_buffer                        buffer;
      http::response_parser<http::dynamic_body> parser;
      parser.body_limit((std::numeric_limits<std::uint64_t>::max)()); // No body size limit
      parser.get().body().clear();
      parser.get().body().shrink_to_fit();

      beast::error_code ec;
      // Read header first
      http::read_header(*m_socket, buffer, parser, ec);
      if (ec)
      {
        log::ERROR<log::NET>("Failed to read headers: {}", ec.message());
        return false;
      }

      // Check if transfer-encoding is chunked
      if (!parser.get().chunked())
      {
        log::WARN<log::NET>("Response not chunked — streaming might not be incremental");
      }

      // Incrementally read chunks
      while (!parser.is_done())
      {
        http::read_some(*m_socket, buffer, parser, ec);

        if (ec == http::error::need_buffer)
        {
          continue; // More data is needed
        }
        else if (ec && ec != asio::error::eof)
        {
          log::ERROR<log::NET>("Error while streaming: {}", ec.message());
          return false;
        }

        // Extract chunk data
        auto& body = parser.get().body();
        for (auto const& seq : body.data())
        {
          const char* data_ptr = static_cast<const char*>(seq.data());
          std::size_t data_len = seq.size();
          if (data_len > 0 && on_chunk)
          {
            on_chunk(data_ptr, data_len);
          }
        }
        body.clear();
      }

      beast::error_code shutdown_ec;
      m_socket->shutdown(shutdown_ec);
      if (shutdown_ec == asio::error::eof)
        shutdown_ec.clear();

      return true;
    }
    catch (const std::exception& e)
    {
      log::ERROR<log::NET>("Chunked streaming failed: {}", e.what());
      return false;
    }
  }

private:
  asio::io_context&    m_ioCtx;
  ssl::context&        m_sslCtx;
  IPAddr               m_server;
  std::chrono::seconds m_timeout{10}; // default 10s

  std::unique_ptr<beast::ssl_stream<tcp::socket>> m_socket;

  auto makeRequest(http::verb method, const NetTarget& target, const std::string& body)
    -> NetResponse
  {
    try
    {
      tcp::resolver resolver{m_ioCtx};
      m_socket = std::make_unique<beast::ssl_stream<tcp::socket>>(m_ioCtx, m_sslCtx);

      auto results = resolver.resolve(m_server, WAVY_SERVER_PORT_NO_STR);
      asio::connect(beast::get_lowest_layer(*m_socket), results.begin(), results.end());

      m_socket->handshake(ssl::stream_base::client);

      // Timeout
      asio::steady_timer timer(m_ioCtx);
      timer.expires_after(m_timeout);

      bool timed_out = false;
      timer.async_wait(
        [&](const beast::error_code& ec)
        {
          if (!ec && m_socket)
          {
            timed_out = true;
            cancelCurrentRequest();
          }
        });

      // Request
      http::request<http::string_body> req{method, target, 11};
      req.set(http::field::host, m_server);
      req.set(http::field::user_agent, "WavyClient");

      if (method == http::verb::post)
      {
        req.set(http::field::content_type, macros::CONTENT_TYPE_JSON);
        req.body() = body;
        req.prepare_payload();
      }

      http::write(*m_socket, req);

      beast::flat_buffer                 buffer;
      http::response<http::dynamic_body> res;
      http::read(*m_socket, buffer, res);

      timer.cancel(); // Cancel timer after success

      if (timed_out)
      {
        log::ERROR<log::NET>("Request timed out");
        return "";
      }

      NetResponse response_data = beast::buffers_to_string(res.body().data());

      beast::error_code ec;
      m_socket->shutdown(ec);
      if (ec == asio::error::eof)
        ec.clear();

      return response_data;
    }
    catch (const std::exception& e)
    {
      log::ERROR<log::NET>("HTTPS request failed: {}", e.what());
      return "";
    }
  }
};

} // namespace libwavy::network
