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
#include <chrono>
#include <iostream>
#include <random>
#include <utility>

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
      : resolver_(ioc), socket_(ioc), server_url_(std::move(server_url))
  {
  }

  auto diagnoseNetworkSpeed() -> NetworkStats
  {
    std::string host, port, target;
    parseUrl(server_url_, host, port, target);

    NetworkStats stats = {
      .latency = -1, .jitter = 0.0, .packet_loss = 0.0}; // Default invalid values

    try
    {
      std::cout << "[INFO] Diagnosing network speed to: " << host << "\n";
      auto const results = resolver_.resolve(host, port);

      std::vector<int> latencies;

      for (int i = 0; i < 5; ++i)
      { // Send 5 probes to calculate jitter
        auto start = high_resolution_clock::now();
        socket_.connect(*results.begin());
        auto end       = high_resolution_clock::now();
        int  ping_time = duration_cast<milliseconds>(end - start).count();
        latencies.push_back(ping_time);
        socket_.close();
      }

      stats.latency     = calculateAverage(latencies);
      stats.jitter      = calculateJitter(latencies);
      stats.packet_loss = simulatePacketLoss(); // Simulating packet loss

      std::cout << "[INFO] Network Latency: " << stats.latency << " ms\n";
      std::cout << "[INFO] Network Jitter: " << stats.jitter << " ms\n";
      std::cout << "[INFO] Packet Loss: " << stats.packet_loss << "%\n";
    }
    catch (const std::exception& e)
    {
      std::cerr << "[ERROR] Network speed diagnosis failed: " << e.what() << "\n";
    }

    return stats;
  }

private:
  tcp::resolver resolver_;
  tcp::socket   socket_;
  std::string   server_url_;

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
      port = "443"; // Default HTTPS port
    }

    target = (end == std::string::npos) ? "/" : url.substr(end);
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
    {
      sum += std::abs(latencies[i] - latencies[i - 1]);
    }
    return sum / (latencies.size() - 1);
  }

  auto simulatePacketLoss() -> double
  {
    std::random_device               rd;
    std::mt19937                     gen(rd());
    std::uniform_real_distribution<> dist(0.0, 10.0); // Simulating 0-10% loss
    return dist(gen);
  }
};

} // namespace libwavy::abr
