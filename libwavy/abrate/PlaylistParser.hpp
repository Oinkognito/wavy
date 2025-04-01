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
#include <boost/asio/ssl.hpp>
#include <boost/beast.hpp>
#include <libwavy/common/macros.hpp>
#include <libwavy/logger.hpp>
#include <map>
#include <sstream>
#include <string>
#include <utility>

namespace libwavy::abr
{

namespace beast = boost::beast;
namespace http  = beast::http;
namespace net   = boost::asio;
namespace ssl   = boost::asio::ssl;
using tcp       = net::ip::tcp;

class PlaylistParser
{
public:
  PlaylistParser(net::io_context& ioc, ssl::context& ssl_ctx, std::string url)
      : resolver_(ioc), stream_(ioc, ssl_ctx), master_url_(std::move(url))
  {
  }

  auto fetchMasterPlaylist() -> bool
  {
    std::string host, port, target;
    parseUrl(master_url_, host, port, target);

    LOG_INFO << "Fetching master playlist from: " << master_url_;

    try
    {
      LOG_INFO << "Resolving host: " << host << " on port " << port;
      auto const results = resolver_.resolve(host, port);

      LOG_INFO << "Connecting to: " << host << ":" << port;
      beast::get_lowest_layer(stream_).connect(results);
      stream_.handshake(ssl::stream_base::client);

      http::request<http::string_body> req{http::verb::get, target, 11};
      req.set(http::field::host, host);
      req.set(http::field::user_agent, "Boost.Beast");

      LOG_INFO << "Sending HTTP GET request...";
      http::write(stream_, req);

      beast::flat_buffer                 buffer;
      http::response<http::dynamic_body> res;
      http::read(stream_, buffer, res);

      LOG_INFO << "Received HTTP Response. Status: " << res.result_int();
      beast::error_code ec;
      stream_.shutdown(ec);
      if (ec && ec != beast::errc::not_connected)
      {
        LOG_WARNING << "SSL Shutdown failed: " << ec.message();
      }

      std::string body = beast::buffers_to_string(res.body().data());

      parsePlaylist(body);
      return true;
    }
    catch (const std::exception& e)
    {
      LOG_ERROR << "Error fetching master playlist: " << e.what();
      return false;
    }
  }

  [[nodiscard]] auto getBitratePlaylists() const -> std::map<int, std::string>
  {
    return bitrate_playlists_;
  }

private:
  net::ip::tcp::resolver         resolver_;
  ssl::stream<beast::tcp_stream> stream_;
  std::string                    master_url_;
  std::map<int, std::string>     bitrate_playlists_;

  void parseUrl(const std::string& url, std::string& host, std::string& port, std::string& target)
  {
    size_t pos   = url.find("//");
    size_t start = (pos == std::string::npos) ? 0 : pos + 2;
    size_t end   = url.find('/', start);

    std::string full_host = url.substr(start, end - start);
    size_t      port_pos  = full_host.find(':');

    if (port_pos != std::string::npos)
    {
      host = full_host.substr(0, port_pos);
      port = full_host.substr(port_pos + 1);
    }
    else
    {
      host = full_host;
      port = WAVY_SERVER_PORT_NO_STR; // Default HTTPS port
    }

    target = (end == std::string::npos) ? "/" : url.substr(end);
    LOG_DEBUG << "Parsed Host: " << host << ", Port: " << port << ", Target: " << target;
  }

  void parsePlaylist(const std::string& playlist)
  {
    std::istringstream ss(playlist);
    std::string        line;
    int                current_bitrate = 0;

    while (std::getline(ss, line))
    {
      if (line.find("#EXT-X-STREAM-INF") != std::string::npos)
      {
        size_t bitrate_pos = line.find("BANDWIDTH=");
        if (bitrate_pos != std::string::npos)
        {
          size_t start = bitrate_pos + 9;       // Position after "BANDWIDTH="
          size_t end   = line.find(',', start); // Find next comma

          std::string bitrate_str = line.substr(start, end - start);

          // Remove any '=' sign if it appears (edge case)
          std::erase(bitrate_str, '=');

          LOG_DEBUG << "Extracted Bitrate String: '" << bitrate_str << "'";

          // Ensure it's numeric
          if (!bitrate_str.empty() && std::ranges::all_of(bitrate_str, ::isdigit))
          {
            try
            {
              current_bitrate = std::stoi(bitrate_str);
              LOG_INFO << "Parsed Bitrate: " << current_bitrate << " kbps";
            }
            catch (const std::exception& e)
            {
              LOG_ERROR << "Failed to parse bitrate: " << e.what();
              continue;
            }
          }
          else
          {
            LOG_ERROR << "Invalid bitrate format: '" << bitrate_str << "'";
            continue;
          }
        }
      }
      else if (!line.empty() && line[0] != '#')
      {
        if (current_bitrate > 0)
        {
          bitrate_playlists_[current_bitrate] = line;
          LOG_INFO << "Added bitrate playlist: " << current_bitrate << " -> " << line;
        }
        else
        {
          LOG_ERROR << "Playlist URL found but no valid bitrate!";
        }
      }
    }
  }
};

} // namespace libwavy::abr
