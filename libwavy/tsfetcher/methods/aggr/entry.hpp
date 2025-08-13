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

#include <cstdlib>
#include <sstream>
#include <string>
#include <utility>

#include <libwavy/common/macros.hpp>
#include <libwavy/common/network/routes.h>
#include <libwavy/common/state.hpp>
#include <libwavy/common/types.hpp>
#include <libwavy/log-macros.hpp>
#include <libwavy/network/entry.hpp>
#include <libwavy/tsfetcher/interface.hpp>
#include <libwavy/utils/audio/entry.hpp>
#include <libwavy/utils/io/dbg/entry.hpp>
#include <memory>

#include <indicators/cursor_control.hpp>
#include <indicators/progress_bar.hpp>

namespace libwavy::fetch
{

class AggrSegFetcher : public ISegmentFetcher
{
public:
  AggrSegFetcher(IPAddr server)
      : m_ioCtx(std::make_shared<asio::io_context>()),
        m_sslCtx(std::make_shared<ssl::context>(ssl::context::tlsv12_client)),
        m_serverIP(std::move(server))
  {
    m_sslCtx->set_verify_mode(ssl::verify_none);
  }

  auto fetchAndPlay(const StorageOwnerID& nickname, const StorageAudioID& audio_id,
                    int desired_bandwidth, bool& flac_found, const RelPath& audio_backend_lib_path)
    -> bool override
  {
    try
    {
      log::INFO<log::FETCH>("Request Owner: {}", nickname);
      log::INFO<log::FETCH>("Audio-ID: {}", audio_id);
      log::INFO<log::FETCH>("Bitrate: {}", desired_bandwidth);

      auto master_playlist = fetch_master_playlist(nickname, audio_id);
      if (master_playlist.empty())
        return false;

      auto content = select_playlist(nickname, audio_id, master_playlist, desired_bandwidth);
      if (content.empty())
        return false;

      AudioData      init_mp4_data;
      TotalAudioData m4s_segments;
      auto gs = process_segments(content, nickname, audio_id, init_mp4_data, m4s_segments);
      if (!gs)
      {
        log::ERROR<log::FETCH>(
          "GlobalState found to be empty after processing segments! Exiting...");
        return false;
      }

      if (!m4s_segments.empty())
      {
        flac_found = true;
        gs->appendSegments(std::move(m4s_segments));
      }

      if (!libwavy::dbg::FileWriter<AudioData>::write(gs->getAllSegments(), "audio.raw"))
      {
        log::ERROR<log::FETCH>("Error writing transport segments to file!!");
        return false;
      }

      log::INFO<log::FETCH>("Stored {} transport segments in memory.", gs->segSizeAll());

      auto allSegments = gs->getAllSegments();

      // Decode and play the fetched stream
      if (!utils::audio::decodeAndPlay(allSegments, flac_found, audio_backend_lib_path))
      {
        return WAVY_RET_FAIL;
      }

      return true;
    }
    catch (std::exception& e)
    {
      log::ERROR<log::FETCH>("fetchAndPlay threw error: {}", e.what());
    }

    return false;
  }

  auto fetchOwnersList(const IPAddr& server, const StorageOwnerID& targetNickname)
    -> Owners override
  {
    asio::io_context ioc;
    ssl::context     ctx(ssl::context::tlsv12_client);
    ctx.set_verify_mode(ssl::verify_none);
    libwavy::network::HttpsClient client(ioc, ctx, server);

    log::TRACE<log::FETCH>("Attempting to fetch Owners list of owner {} through Wavy-Server at {}",
                           targetNickname, server);

    NetResponse response = client.get(routes::SERVER_PATH_OWNERS);

    if (response.empty())
      return {};

    std::istringstream iss(response);
    std::string        line;
    StorageOwnerID     currentNickname;
    Owners             owners;

    while (std::getline(iss, line))
    {
      if (line.empty())
        continue;

      if (line.find(':') != std::string::npos)
      {
        currentNickname = line.substr(0, line.find(':')); // Extract the Owner Nickname
        continue;
      }

      if (currentNickname == targetNickname) // Filter by the provided Owner Nickname
      {
        size_t pos = line.find("- ");
        if (pos != std::string::npos)
        {
          StorageOwnerID client_id = line.substr(pos + 2); // Extract client ID
          owners.push_back(client_id);
        }
      }
    }

    return owners;
  }

private:
  std::shared_ptr<asio::io_context> m_ioCtx;
  std::shared_ptr<ssl::context>     m_sslCtx;
  IPAddr                            m_serverIP;

  auto make_client() -> std::unique_ptr<libwavy::network::HttpsClient>
  {
    return std::make_unique<libwavy::network::HttpsClient>(*m_ioCtx, *m_sslCtx, m_serverIP);
  }

  auto fetch_master_playlist(const StorageOwnerID& nickname, const StorageAudioID& audio_id)
    -> PlaylistData
  {
    const NetTarget masterPlaylistPath =
      "/download/" + nickname + "/" + audio_id + "/" + macros::to_string(macros::MASTER_PLAYLIST);

    log::DBG<log::FETCH>("Fetching Master Playlist from: '{}'", masterPlaylistPath);

    auto         client  = make_client();
    PlaylistData content = client->get(masterPlaylistPath);
    if (content.empty())
    {
      log::ERROR<log::FETCH>("Failed to fetch master playlist for Owner + audio ID: {}/{}",
                             nickname, audio_id);
    }
    return content;
  }

  auto select_playlist(const StorageOwnerID& nickname, const std::string& audio_id,
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
      log::WARN<log::FETCH>("Exact match not found. Using max bitrate: {} BPS", max_bandwidth);
    }

    const NetTarget playlist_path = "/download/" + nickname + "/" + audio_id + "/" + selected;
    log::INFO<log::FETCH>("Selected bitrate playlist: {}", playlist_path);

    auto client = make_client();
    return client->get(playlist_path);
  }

  auto process_segments(const PlaylistData& playlist, const StorageOwnerID& nickname,
                        const StorageAudioID& audio_id, AudioData& init_mp4_data,
                        TotalAudioData& m4s_segments) -> std::unique_ptr<GlobalState>
  {
    std::istringstream stream(playlist);
    std::string        line;
    bool               has_m4s = false;

    // Check if playlist contains m4s files
    while (std::getline(stream, line))
    {
      if (!line.empty() && line[0] != '#' && line.ends_with(macros::M4S_FILE_EXT))
      {
        has_m4s = true;
        break;
      }
    }

    auto                         client = make_client();
    std::unique_ptr<GlobalState> gs     = nullptr;

    // Fetch init.mp4 if needed
    if (has_m4s)
    {
      const NetTarget initMp4Url = "/download/" + nickname + "/" + audio_id + "/init.mp4";
      init_mp4_data              = client->get(initMp4Url);
      if (init_mp4_data.empty())
      {
        log::ERROR<log::FETCH>("Failed to fetch init.mp4 for {}/{}", nickname, audio_id);
        return nullptr;
      }
      log::INFO<log::FETCH>("Fetched init.mp4, size: {} bytes.", init_mp4_data.size());
      gs = std::make_unique<GlobalState>(std::move(init_mp4_data)); // construct with init
    }
    else
    {
      gs = std::make_unique<GlobalState>(); // default constructor for MP3 case
    }

    // Rewind stream and collect all segment lines
    stream.clear();
    stream.seekg(0);

    std::vector<std::string> segment_lines;
    while (std::getline(stream, line))
    {
      if (!line.empty() && line[0] != '#')
      {
        segment_lines.push_back(line);
      }
    }

    if (segment_lines.empty())
    {
      log::WARN<log::FETCH>("No segments found in playlist for {}/{}", nickname, audio_id);
      return gs;
    }

    // Setup progress bar
    indicators::ProgressBar bar{indicators::option::BarWidth{40},
                                indicators::option::Start{"["},
                                indicators::option::Fill{"="},
                                indicators::option::Lead{">"},
                                indicators::option::Remainder{" "},
                                indicators::option::End{"]"},
                                indicators::option::ForegroundColor{indicators::Color::blue},
                                indicators::option::ShowElapsedTime{true},
                                indicators::option::ShowRemainingTime{true},
                                indicators::option::MaxProgress{segment_lines.size()}};

    auto   start_time       = std::chrono::steady_clock::now();
    size_t fetched_segments = 0;
    size_t total_segments   = segment_lines.size();

    // Fetch each segment
    for (const auto& seg_line : segment_lines)
    {
      const NetTarget url = "/download/" + nickname + "/" + audio_id + "/" + seg_line;
      log::TRACE<log::FETCH>("Fetching URL: {}", url);

      auto      seg_start = std::chrono::steady_clock::now();
      AudioData data      = client->get(url);
      auto      seg_end   = std::chrono::steady_clock::now();

      if (!data.empty())
      {
        if (seg_line.ends_with(macros::M4S_FILE_EXT))
        {
          m4s_segments.push_back(std::move(data));
        }
        else if (seg_line.ends_with(macros::TRANSPORT_STREAM_EXT))
        {
          gs->appendSegment(std::move(data));
        }

        log::DBG<log::FETCH>("Fetched segment: {}", seg_line);
      }
      else
      {
        log::WARN<log::FETCH>("Failed to fetch segment: {}", seg_line);
      }

      // Update progress info
      fetched_segments++;
      double avg_time_per_seg =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - start_time).count() /
        fetched_segments;

      bar.set_progress(fetched_segments);
      bar.set_option(indicators::option::PrefixText{
        std::format("Segments: {}/{} ETA: {:.1f}s", fetched_segments, total_segments,
                    avg_time_per_seg * (total_segments - fetched_segments))});
    }

    bar.mark_as_completed();
    return gs;
  }
};

} // namespace libwavy::fetch
