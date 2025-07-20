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

#include <libwavy/common/macros.hpp>
#include <libwavy/logger.hpp>
#include <libwavy/network/entry.hpp>
#include <libwavy/parser/ast/entry.hpp>
#include <libwavy/parser/entry.hpp>
#include <libwavy/parser/macros.hpp>
#include <map>
#include <string>
#include <utility>

namespace libwavy::abr
{

namespace beast = boost::beast;
namespace http  = beast::http;
namespace net   = boost::asio;
namespace ssl   = boost::asio::ssl;
namespace bfs   = boost::filesystem;
using tcp       = net::ip::tcp;

using namespace libwavy::hls::parser;

class PlaylistParser
{
public:
  PlaylistParser(net::io_context& ioc, ssl::context& ssl_ctx, std::string url)
      : ioc_(ioc), ssl_ctx_(ssl_ctx), master_url_(std::move(url))
  {
  }

  auto fetchMasterPlaylist() -> bool
  {
    std::string host, port, target;
    parseUrl(master_url_, host, port, target);

    LOG_INFO << "Fetching master playlist from: " << target;

    try
    {
      libwavy::network::HttpsClient client_(ioc_, ssl_ctx_, host);
      LOG_INFO << "Resolving host: " << host << " on port " << port;
      std::string body = client_.get(target);

      try
      {
        LOG_INFO << "Parsing master playlist using template HLS parser";
        std::string base_url = getBaseUrl(master_url_);
        master_playlist_     = M3U8Parser::parseMasterPlaylist(body, base_url);

        updateBitratePlaylistsFromAst();

        // Print the AST for debugging
        printAST(master_playlist_);

        return true;
      }
      catch (const std::exception& e)
      {
        LOG_ERROR << "Failed to parse master playlist: " << e.what();
        return false;
      }
    }
    catch (const std::exception& e)
    {
      LOG_ERROR << "Error fetching master playlist: " << e.what();
      return false;
    }
  }

  auto fetchMediaPlaylist(int bitrate) -> bool
  {
    auto it = bitrate_playlists_.find(bitrate);
    if (it == bitrate_playlists_.end())
    {
      LOG_ERROR << "No playlist found for bitrate: " << bitrate;
      return false;
    }

    std::string url = it->second;
    // Handle relative URLs
    if (!url.starts_with("http"))
    {
      std::string base_url = getBaseUrl(master_url_);
      url                  = base_url + (url.starts_with("/") ? "" : "/") + url;
    }

    std::string host, port, target;
    parseUrl(url, host, port, target);

    LOG_INFO << "Fetching media playlist for bitrate " << bitrate << " from: " << target;

    try
    {
      libwavy::network::HttpsClient client_(ioc_, ssl_ctx_, host);
      LOG_INFO << "Resolving host: " << host << " on port " << port;
      std::string body = client_.get(target);

      try
      {
        LOG_INFO << "Parsing media playlist using template HLS parser";
        std::string base_url      = getBaseUrl(url);
        media_playlists_[bitrate] = M3U8Parser::parseMediaPlaylist(body, bitrate, base_url);

        printAST(media_playlists_[bitrate]);

        return true;
      }
      catch (const std::exception& e)
      {
        LOG_ERROR << "Failed to parse media playlist: " << e.what();
        return false;
      }
    }
    catch (const std::exception& e)
    {
      LOG_ERROR << "Error fetching media playlist: " << e.what();
      return false;
    }
  }

  [[nodiscard]] auto getBitratePlaylists() const -> std::map<int, std::string>
  {
    return bitrate_playlists_;
  }

  [[nodiscard]] auto getMasterPlaylist() const -> const libwavy::hls::parser::ast::MasterPlaylist&
  {
    return master_playlist_;
  }

  [[nodiscard]] auto getMediaPlaylist(int bitrate) const
    -> std::optional<libwavy::hls::parser::ast::MediaPlaylist>
  {
    auto it = media_playlists_.find(bitrate);
    if (it != media_playlists_.end())
    {
      return it->second;
    }
    return std::nullopt;
  }

private:
  net::io_context&                                        ioc_;
  ssl::context&                                           ssl_ctx_;
  std::string                                             master_url_;
  std::map<int, std::string>                              bitrate_playlists_;
  libwavy::hls::parser::ast::MasterPlaylist               master_playlist_;
  std::map<int, libwavy::hls::parser::ast::MediaPlaylist> media_playlists_;

  auto getBaseUrl(const std::string& url) -> std::string
  {
    size_t pos      = url.find("//");
    size_t start    = (pos == std::string::npos) ? 0 : pos + 2;
    size_t path_pos = url.find('/', start);

    if (path_pos == std::string::npos)
    {
      return url;
    }

    // Find last slash to get the directory
    size_t last_slash = url.rfind('/');
    if (last_slash != std::string::npos)
    {
      return url.substr(0, last_slash);
    }

    return url.substr(0, path_pos);
  }

  void updateBitratePlaylistsFromAst()
  {
    for (const auto& variant : master_playlist_.variants)
    {
      if (variant.bitrate > 0)
      {
        bitrate_playlists_[variant.bitrate] = variant.uri;
        LOG_INFO << "Added bitrate playlist from AST: " << variant.bitrate << " -> " << variant.uri;
      }
    }
  }

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
};

} // namespace libwavy::abr
