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

#include <chrono>
#include <cmath>
#include <future>
#include <libwavy/common/macros.hpp>
#include <libwavy/common/types.hpp>
#include <libwavy/log-macros.hpp>
#include <libwavy/network/entry.hpp>
#include <vector>

namespace libwavy::abr
{

namespace net = boost::asio;
using tcp     = net::ip::tcp;
using namespace std::chrono;

struct NetworkStats
{
  int    latency;     // In milliseconds
  double jitter;      // In milliseconds
  double packet_loss; // Percentage (0-100%)
};

class NetworkDiagnoser
{
public:
  NetworkDiagnoser(net::io_context& ioc, std::string server_url)
      : resolver_(ioc), socket_(ioc), timer_(ioc), server_url_(std::move(server_url))
  {
  }

  auto diagnoseNetworkSpeed() -> NetworkStats
  {
    std::string  host, port;
    NetTarget    target = parseUrl(server_url_, host, port);
    NetworkStats stats  = {.latency = -1, .jitter = 0.0, .packet_loss = 0.0};

    try
    {
      std::vector<int> latencies;
      int              failed_pings = 0;

      std::vector<std::future<int>> futures;
      for (int i = 0; i < 5; ++i)
      {
        futures.push_back(std::async(std::launch::async, [&]() { return sendProbe(host, 2000); }));
      }

      for (auto& future : futures)
      {
        int ping_time = future.get();
        if (ping_time >= 0)
          latencies.push_back(ping_time);
        else
          ++failed_pings;
      }

      if (!latencies.empty())
      {
        stats.latency = calculateAverage(latencies);
        stats.jitter  = calculateJitter(latencies);
      }

      stats.packet_loss = (failed_pings / 5.0) * 100.0;
    }
    catch (const std::exception& e)
    {
      log::ERROR<log::NET>("Network diagnosis failed: {}", e.what());
    }

    return stats;
  }

private:
  tcp::resolver                     resolver_;
  tcp::socket                       socket_;
  net::steady_timer                 timer_;
  std::string                       server_url_;
  time_point<high_resolution_clock> start_time_;

  auto parseUrl(NetTarget& url, IPAddr& host, std::string& port) -> NetTarget
  {
    size_t      pos       = url.find("//");
    size_t      start     = (pos == std::string::npos) ? 0 : pos + 2;
    size_t      end       = url.find('/', start);
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
      port = WAVY_SERVER_PORT_NO_STR; // fallback to wavys default port
    }

    NetTarget target = (end == std::string::npos) ? "/" : url.substr(end);

    return target;
  }

  auto sendProbe(const IPAddr& server, int timeout_ms) -> int
  {
    asio::io_context ioc;
    ssl::context     ctx{ssl::context::sslv23_client};

    libwavy::network::HttpsClient client(ioc, ctx, server);

    auto start_time = std::chrono::high_resolution_clock::now();

    std::future<std::string> future =
      std::async(std::launch::async,
                 [&]()
                 {
                   return client.get(
                     macros::to_string(macros::SERVER_PATH_PING)); // Assuming a simple ping request
                 });

    if (future.wait_for(std::chrono::milliseconds(timeout_ms)) == std::future_status::timeout)
    {
      return -1;
    }

    auto end_time = std::chrono::high_resolution_clock::now();

    return std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
  }

  auto calculateAverage(const std::vector<int>& values) -> int
  {
    int sum = 0;
    for (int v : values)
      sum += v;
    return values.empty() ? -1 : sum / values.size();
  }

  auto calculateJitter(const std::vector<int>& latencies) -> double
  {
    if (latencies.size() < 2)
      return 0.0;
    double sum = 0.0;
    for (size_t i = 1; i < latencies.size(); ++i)
      sum += std::pow(latencies[i] - latencies[i - 1], 2);
    return std::sqrt(sum / (latencies.size() - 1));
  }
};

} // namespace libwavy::abr
