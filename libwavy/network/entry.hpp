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

using Network = libwavy::log::NET;

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

  void get_chunked(const std::string& target, std::function<void(const std::string&)> on_chunk)
  {
    try
    {
      asio::io_context ioc;
      ssl::context     ctx(ssl::context::sslv23_client);
      ctx.set_verify_mode(ssl::verify_none); // allow self-signed for now

      tcp::resolver                        resolver(ioc);
      beast::ssl_stream<beast::tcp_stream> stream(ioc, ctx);

      auto const results = resolver.resolve(m_server, WAVY_SERVER_PORT_NO_STR);

      beast::get_lowest_layer(stream).connect(results);

      if (!SSL_set_tlsext_host_name(stream.native_handle(), m_server.c_str()))
      {
        beast::error_code ec{static_cast<int>(::ERR_get_error()), asio::error::get_ssl_category()};
        throw beast::system_error{ec};
      }

      stream.handshake(ssl::stream_base::client);

      // Send raw GET request
      std::string req = "GET " + target +
                        " HTTP/1.1\r\n"
                        "Host: " +
                        m_server +
                        "\r\n"
                        "User-Agent: WavyClient/1.0\r\n"
                        "Accept: */*\r\n"
                        "Connection: close\r\n\r\n";
      asio::write(stream, asio::buffer(req));

      // Read headers
      asio::streambuf header_buf;
      asio::read_until(stream, header_buf, "\r\n\r\n");

      std::istream header_stream(&header_buf);
      std::string  status_line;
      std::getline(header_stream, status_line);
      if (!status_line.empty() && status_line.back() == '\r')
        status_line.pop_back();

      std::string header_line;
      while (std::getline(header_stream, header_line) && header_line != "\r")
      {
        if (!header_line.empty() && header_line.back() == '\r')
          header_line.pop_back();
        log::DBG<Network>("Header: {}", header_line);
      }

      asio::streambuf buffer;
      if (header_buf.size() > 0)
        buffer.commit(asio::buffer_copy(buffer.prepare(header_buf.size()), header_buf.data()));

      // Read chunks
      std::size_t total_bytes = 0;
      while (true)
      {
        asio::read_until(stream, buffer, CRLF);
        std::istream size_stream(&buffer);
        std::string  chunk_size_line;
        std::getline(size_stream, chunk_size_line);
        if (!chunk_size_line.empty() && chunk_size_line.back() == '\r')
          chunk_size_line.pop_back();

        std::size_t chunk_size = 0;
        try
        {
          chunk_size = std::stoul(chunk_size_line, nullptr, 16);
        }
        catch (...)
        {
          log::ERROR<Network>("Invalid chunk size '{}'", chunk_size_line);
          break;
        }

        if (chunk_size == 0)
        {
          asio::read_until(stream, buffer, CRLF);
          break;
        }

        while (buffer.size() < chunk_size + 2)
          asio::read(stream, buffer, asio::transfer_at_least(chunk_size + 2 - buffer.size()));

        std::string chunk_data(chunk_size, '\0');
        buffer.sgetn(&chunk_data[0], chunk_size);

        on_chunk(chunk_data);

        buffer.consume(2);
        total_bytes += chunk_size;
        log::DBG<Network>("Read {} bytes (total: {})", chunk_size, total_bytes);
      }

      beast::error_code ec;
      stream.shutdown(ec);
      if (ec == asio::error::eof || ec == ssl::error::stream_truncated)
        ec = {};
      if (ec)
        throw beast::system_error{ec};
    }
    catch (std::exception& e)
    {
      log::ERROR<Network>("Exception in get_chunked: {}", e.what());
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
        log::ERROR<Network>("Request timed out");
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
      log::ERROR<Network>("HTTPS request failed: {}", e.what());
      return "";
    }
  }
};

} // namespace libwavy::network
