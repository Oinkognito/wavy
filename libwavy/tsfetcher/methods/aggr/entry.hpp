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

#include <cstdlib>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include <libwavy/common/macros.hpp>
#include <libwavy/common/state.hpp>
#include <libwavy/common/types.hpp>
#include <libwavy/network/entry.hpp>
#include <libwavy/tsfetcher/interface.hpp>
#include <libwavy/utils/audio/entry.hpp>
#include <libwavy/utils/io/dbg/entry.hpp>
#include <memory>

namespace libwavy::fetch
{

class TSegFetcher : public ISegmentFetcher
{
public:
  TSegFetcher(IPAddr server)
      : ioc_(std::make_shared<asio::io_context>()),
        ctx_(std::make_shared<ssl::context>(ssl::context::tlsv12_client)),
        server_(std::move(server))
  {
    ctx_->set_verify_mode(ssl::verify_none);
  }

  auto fetchAndPlay(const StorageOwnerID& ip_id, const StorageAudioID& audio_id, GlobalState& gs,
                    int desired_bandwidth, bool& flac_found, const RelPath& audio_backend_lib_path)
    -> bool override
  {
    LOG_INFO << FETCH_LOG << "Request Owner: " << ip_id;
    LOG_INFO << FETCH_LOG << "Audio-ID: " << audio_id;
    LOG_INFO << FETCH_LOG << "Bitrate: " << desired_bandwidth;

    auto master_playlist = fetch_master_playlist(ip_id, audio_id);
    std::cout << master_playlist << std::endl;
    if (master_playlist.empty())
      return false;

    auto content = select_playlist(ip_id, audio_id, master_playlist, desired_bandwidth);
    if (content.empty())
      return false;

    AudioData      init_mp4_data;
    TotalAudioData m4s_segments;
    if (!process_segments(content, ip_id, audio_id, init_mp4_data, m4s_segments, gs))
      return false;

    if (!m4s_segments.empty())
    {
      flac_found = true;
      gs.appendSegmentsFLAC(std::move(init_mp4_data), std::move(m4s_segments));
    }

    if (!libwavy::dbg::FileWriter<AudioData>::write(gs.getAllSegments(), "audio.raw"))
    {
      LOG_ERROR << FETCH_LOG << "Error writing transport segments to file";
      return false;
    }

    LOG_INFO << FETCH_LOG << "Stored " << gs.segSizeAll() << " transport segments.";

    // Decode and play the fetched stream
    if (!decodeAndPlay(gs, flac_found, audio_backend_lib_path))
    {
      return WAVY_RET_FAIL;
    }

    return true;
  }

  auto fetch_client_list(const IPAddr& server, const StorageOwnerID& target_ip_id)
    -> std::vector<std::string> override
  {
    asio::io_context ioc;
    ssl::context     ctx(ssl::context::tlsv12_client);
    ctx.set_verify_mode(ssl::verify_none);
    libwavy::network::HttpsClient client(ioc, ctx, server);

    NetResponse response = client.get(macros::to_string(macros::SERVER_PATH_HLS_CLIENTS));

    if (response.empty())
      return {};

    std::istringstream       iss(response);
    std::string              line;
    StorageOwnerID           current_ip_id;
    std::vector<std::string> clients;

    while (std::getline(iss, line))
    {
      if (line.empty())
        continue;

      if (line.find(':') != std::string::npos)
      {
        current_ip_id = line.substr(0, line.find(':')); // Extract the IP-ID
        continue;
      }

      if (current_ip_id == target_ip_id) // Filter by the provided IP-ID
      {
        size_t pos = line.find("- ");
        if (pos != std::string::npos)
        {
          StorageOwnerID client_id = line.substr(pos + 2); // Extract client ID
          clients.push_back(client_id);
        }
      }
    }

    return clients;
  }

private:
  std::shared_ptr<asio::io_context> ioc_;
  std::shared_ptr<ssl::context>     ctx_;
  IPAddr                            server_;

  auto make_client() -> std::unique_ptr<libwavy::network::HttpsClient>
  {
    return std::make_unique<libwavy::network::HttpsClient>(*ioc_, *ctx_, server_);
  }

  auto fetch_master_playlist(const StorageOwnerID& ip_id, const StorageAudioID& audio_id)
    -> PlaylistData
  {
    NetTarget path =
      "/hls/" + ip_id + "/" + audio_id + "/" + macros::to_string(macros::MASTER_PLAYLIST);
    auto         client  = make_client();
    PlaylistData content = client->get(path);
    if (content.empty())
    {
      LOG_ERROR << FETCH_LOG << "Failed to fetch master playlist for " << ip_id << "/" << audio_id;
    }
    return content;
  }

  auto select_playlist(const StorageOwnerID& ip_id, const std::string& audio_id,
                       const std::string& content, int desired_bandwidth) -> std::string
  {
    if (content.find(macros::PLAYLIST_VARIANT_TAG) == std::string::npos)
      return content;

    std::istringstream iss(content);
    std::string        line, selected;
    int                max_bandwidth = 0;
    std::string        max_bandwidth_url;

    while (std::getline(iss, line))
    {
      if (line.find(macros::PLAYLIST_VARIANT_TAG) != std::string::npos)
      {
        auto pos = line.find("BANDWIDTH=");
        if (pos != std::string::npos)
        {
          int bandwidth = std::stoi(line.substr(pos + 10, line.find_first_of(", ", pos + 10)));
          std::string nextLine;
          if (std::getline(iss, nextLine))
          {
            if (bandwidth == desired_bandwidth)
            {
              selected = nextLine;
              break;
            }
            if (bandwidth > max_bandwidth)
            {
              max_bandwidth     = bandwidth;
              max_bandwidth_url = nextLine;
            }
          }
        }
      }
    }

    if (selected.empty())
    {
      selected = max_bandwidth_url;
      LOG_WARNING << FETCH_LOG << "Exact match not found. Using max bitrate: " << max_bandwidth;
    }

    NetTarget playlist_path = "/hls/" + ip_id + "/" + audio_id + "/" + selected;
    LOG_INFO << FETCH_LOG << "Selected bitrate playlist: " << playlist_path;

    auto client = make_client();
    return client->get(playlist_path);
  }

  auto process_segments(const PlaylistData& playlist, const StorageOwnerID& ip_id,
                        const StorageAudioID& audio_id, AudioData& init_mp4_data,
                        TotalAudioData& m4s_segments, GlobalState& gs) -> bool
  {
    std::istringstream stream(playlist);
    std::string        line;
    bool               has_m4s = false;

    while (std::getline(stream, line))
    {
      if (!line.empty() && line[0] != '#' && line.ends_with(macros::M4S_FILE_EXT))
      {
        has_m4s = true;
        break;
      }
    }

    auto client = make_client();

    if (has_m4s)
    {
      NetTarget initMp4Url = "/hls/" + ip_id + "/" + audio_id + "/init.mp4";
      init_mp4_data        = client->get(initMp4Url);
      if (init_mp4_data.empty())
      {
        LOG_ERROR << FETCH_LOG << "Failed to fetch init.mp4 for " << ip_id << "/" << audio_id;
        return false;
      }
      LOG_INFO << FETCH_LOG << "Fetched init.mp4, size: " << init_mp4_data.size() << " bytes.";
    }

    stream.clear();
    stream.seekg(0);
    while (std::getline(stream, line))
    {
      if (!line.empty() && line[0] != '#')
      {
        NetTarget url = "/hls/" + ip_id + "/" + audio_id + "/" + line;
        LOG_TRACE << FETCH_LOG << "Fetching URL: " << url;
        AudioData data = client->get(url);

        if (!data.empty())
        {
          if (line.ends_with(macros::M4S_FILE_EXT))
            m4s_segments.push_back(std::move(data));
          else if (line.ends_with(macros::TRANSPORT_STREAM_EXT))
            gs.appendSegment(std::move(data));

          LOG_DEBUG << FETCH_LOG << "Fetched segment: " << line;
        }
        else
        {
          LOG_WARNING << FETCH_LOG << "Failed to fetch segment: " << line;
        }
      }
    }

    return true;
  }
};

} // namespace libwavy::fetch
