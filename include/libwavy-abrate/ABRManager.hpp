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

#include "NetworkDiagnoser.hpp"
#include "PlaylistParser.hpp"
#include <algorithm>
#include <vector>

namespace libwavy::abr
{

class ABRManager
{
public:
  ABRManager(boost::asio::io_context& ioc, const std::string& master_url)
      : ioc_(ioc), ssl_ctx_(boost::asio::ssl::context::sslv23), parser_(ioc, ssl_ctx_, master_url),
        network_(ioc, master_url)
  {
  }

  void selectBestBitrate()
  {
    if (!parser_.fetchMasterPlaylist())
    {
      std::cerr << "[ERROR] Failed to fetch master playlist.\n";
      return;
    }

    NetworkStats stats = network_.diagnoseNetworkSpeed();
    if (stats.latency < 0)
    {
      std::cerr << "[ERROR] Network diagnosis failed.\n";
      return;
    }

    std::map<int, std::string> playlists = parser_.getBitratePlaylists();
    if (playlists.empty())
    {
      std::cerr << "[ERROR] No available bitrates in playlist.\n";
      return;
    }

    // Extract bitrates and sort them in ascending order
    std::vector<int> available_bitrates;
    for (const auto& [bitrate, _] : playlists)
    {
      available_bitrates.push_back(bitrate);
    }
    std::sort(available_bitrates.begin(), available_bitrates.end());

    int selected_bitrate = determineBestBitrate(stats, available_bitrates);
    std::cout << "[INFO] Selected Best Bitrate: " << selected_bitrate << " kbps\n";
  }

private:
  boost::asio::io_context&  ioc_;
  boost::asio::ssl::context ssl_ctx_;
  PlaylistParser            parser_;
  NetworkDiagnoser          network_;

  /**
     * Determines the best bitrate based on network statistics.
     */
  int determineBestBitrate(const NetworkStats& stats, const std::vector<int>& bitrates)
  {
    if (bitrates.empty())
      return 64000; // Default minimum bitrate

    // Define network thresholds
    constexpr int    LOW_LATENCY     = 80;
    constexpr int    MEDIUM_LATENCY  = 150;
    constexpr int    HIGH_LATENCY    = 250;
    constexpr double MAX_PACKET_LOSS = 20.0;
    constexpr double MAX_JITTER      = 50.0;

    int selected_bitrate = bitrates.front(); // Start with the lowest bitrate

    for (int bitrate : bitrates)
    {
      if (stats.packet_loss > MAX_PACKET_LOSS || stats.jitter > MAX_JITTER)
      {
        // High packet loss or jitter -> stick to lowest bitrate
        break;
      }
      if (stats.latency < LOW_LATENCY)
      {
        // Low latency -> Choose highest available bitrate
        selected_bitrate = bitrate;
      }
      else if (stats.latency < MEDIUM_LATENCY)
      {
        // Medium latency -> Choose medium bitrate
        if (bitrate <= bitrates[bitrates.size() / 2])
        {
          selected_bitrate = bitrate;
        }
      }
      else if (stats.latency < HIGH_LATENCY)
      {
        // High latency -> Choose lowest available bitrate
        selected_bitrate = bitrates.front();
        break;
      }
    }

    return selected_bitrate;
  }
};

} // namespace libwavy::abr
