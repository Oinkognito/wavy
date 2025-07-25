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

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/beast.hpp>
#include <libwavy/common/macros.hpp>
#include <libwavy/common/types.hpp>
#include <libwavy/log-macros.hpp>
#include <libwavy/server/prototypes.hpp>
#include <utility>

namespace beast = boost::beast;
namespace http  = beast::http;
using boost::asio::ip::tcp;

using ServerDownload = libwavy::log::SERVER_DWNLD;
using Socket         = boost::asio::ssl::stream<tcp::socket>;

namespace libwavy::server::methods
{

class DownloadManager : public std::enable_shared_from_this<DownloadManager>
{
public:
  DownloadManager(Socket&& socket, http::request<http::string_body> req, const IPAddr ipAddr)
      : m_socket(std::move(socket)), m_request(std::move(req)), m_ip(std::move(ipAddr))
  {
  }

  void Run()
  {
    log::INFO<ServerDownload>(LogMode::Async, "Download request from [{}] target: {}", m_ip,
                              NetTarget(m_request.target()));

    const NetTarget target(m_request.target().begin(), m_request.target().end());

    std::istringstream iss(target);

    const auto parts = helpers::tokenizePath(iss);

    if (parts.size() < 4 || parts[0] != "hls")
    {
      log::ERROR<ServerDownload>(LogMode::Async,
                                 "Invalid download path '{}' found! Sending error...", target);
      return SendError(http::status::bad_request, "Invalid download path provided. Try again!");
    }

    const StorageOwnerID owner_id = parts[1];
    const StorageAudioID audio_id = parts[2];
    const FileName       filename = parts[3];

    const AbsPath file_path = macros::to_string(macros::SERVER_STORAGE_DIR) + "/" + owner_id + "/" +
                              audio_id + "/" + filename;

    log::INFO<ServerDownload>(LogMode::Async, "[{}] attempting to serve file: {}", m_ip, file_path);

    m_targetFile = std::make_shared<http::file_body::value_type>();
    boost::system::error_code ec;
    m_targetFile->open(file_path.c_str(), beast::file_mode::scan, ec);

    if (ec)
    {
      return SendError(http::status::not_found, "File not found.");
    }

    const std::size_t file_size    = m_targetFile->size();
    const std::string content_type = std::string(DetectMimeType(filename));

    auto response = std::make_shared<http::response<http::file_body>>();
    response->version(m_request.version());
    response->result(http::status::ok);
    response->set(http::field::server, "Wavy Server");
    response->set(http::field::content_type, content_type);
    response->content_length(file_size);
    response->body() = std::move(*m_targetFile);
    response->prepare_payload();

    log::INFO<ServerDownload>(LogMode::Async, "Serving '{}' ({} bytes) [{}]", filename, file_size,
                              content_type);

    auto self = shared_from_this();
    http::async_write(
      m_socket, *response,
      [this, self, response](boost::system::error_code ec, std::size_t bytes_sent)
      {
        if (ec)
        {
          log::ERROR<ServerDownload>(LogMode::Async, "Write failed: {}", ec.message());
        }
        else
        {
          log::INFO<ServerDownload>(LogMode::Async, "Sent {} bytes to [{}]", bytes_sent, m_ip);
        }

        m_socket.async_shutdown(
          [this, self](boost::system::error_code ec_shutdown)
          {
            if (ec_shutdown)
              log::ERROR<ServerDownload>(LogMode::Async, "Shutdown error: {}",
                                         ec_shutdown.message());
            m_socket.lowest_layer().close();
          });
      });
  }

private:
  void SendError(http::status code, const std::string& body)
  {
    auto response = std::make_shared<http::response<http::string_body>>();
    response->version(m_request.version());
    response->result(code);
    response->set(http::field::server, "Wavy Server");
    response->set(http::field::content_type, "text/plain");
    response->set(http::field::content_length, std::to_string(body.size()));
    response->body() = body;
    response->prepare_payload();

    auto self = shared_from_this();
    http::async_write(m_socket, *response,
                      [this, self, response](boost::system::error_code ec, std::size_t)
                      {
                        if (ec)
                          log::ERROR<ServerDownload>(
                            LogMode::Async, "Error response send failed: {}", ec.message());
                        m_socket.async_shutdown(
                          [this, self](boost::system::error_code ec_shutdown)
                          {
                            if (ec_shutdown)
                              log::ERROR<ServerDownload>(LogMode::Async,
                                                         "Shutdown error (error response): {}",
                                                         ec_shutdown.message());
                            m_socket.lowest_layer().close();
                          });
                      });
  }

  auto DetectMimeType(const std::string& filename) -> std::string_view
  {
    if (filename.ends_with(macros::PLAYLIST_EXT))
      return "application/vnd.apple.mpegurl";
    if (filename.ends_with(macros::TRANSPORT_STREAM_EXT))
      return "video/mp2t";
    return macros::CONTENT_TYPE_OCTET_STREAM;
  }

private:
  Socket                                       m_socket;
  http::request<http::string_body>             m_request;
  std::shared_ptr<http::file_body::value_type> m_targetFile;
  IPAddr                                       m_ip;
};

} // namespace libwavy::server::methods
