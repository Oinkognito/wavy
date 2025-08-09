#pragma once

#include <cstdlib>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include <libwavy/common/macros.hpp>
#include <libwavy/common/state.hpp>
#include <libwavy/common/types.hpp>
#include <libwavy/log-macros.hpp>
#include <libwavy/network/entry.hpp>
#include <libwavy/tsfetcher/interface.hpp>
#include <libwavy/utils/audio/entry.hpp>
#include <libwavy/utils/io/dbg/entry.hpp>
#include <memory>

namespace libwavy::fetch
{

class TSegStreamer : public ISegmentFetcher
{
public:
  TSegStreamer(IPAddr server)
      : ioc_(std::make_shared<asio::io_context>()),
        ctx_(std::make_shared<ssl::context>(ssl::context::tlsv12_client)),
        server_(std::move(server))
  {
    ctx_->set_verify_mode(ssl::verify_none);
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

      std::vector<std::string> segment_list = parse_segment_list(content);
      if (segment_list.empty())
        return false;

      std::unique_ptr<GlobalState> gs = nullptr;
      AudioData                    init_mp4_data;
      auto                         client = make_client();

      if (contains_m4s(segment_list))
      {
        const NetTarget initMp4Url = "/hls/" + nickname + "/" + audio_id + "/init.mp4";
        init_mp4_data              = client->get(initMp4Url);
        if (init_mp4_data.empty())
        {
          log::ERROR<log::FETCH>("Failed to fetch init.mp4 for {}/{}", nickname, audio_id);
          return false;
        }
        flac_found = true;
        gs         = std::make_unique<GlobalState>(std::move(init_mp4_data));
      }
      else
      {
        gs = std::make_unique<GlobalState>();
      }

      std::mutex              mtx;
      std::condition_variable cv;
      AudioData               next_segment;
      bool                    has_next = false;
      bool                    finished = false;

      std::thread playback_thread(
        [&]()
        {
          size_t seg_idx = 0;
          while (true)
          {
            std::unique_lock lock(mtx);
            cv.wait(lock, [&] { return has_next || finished; });

            if (finished && !has_next)
              break;

            log::INFO<log::FETCH>(">>> decodeAndPlay BEGIN for segment [{}]", seg_idx);
            TotalAudioData single_segment{std::move(next_segment)};
            has_next = false;
            lock.unlock();

            if (!utils::audio::decodeAndPlay(single_segment, flac_found, audio_backend_lib_path))
            {
              log::ERROR<log::FETCH>("!!! Playback failed for segment [{}]", seg_idx);
              break;
            }
            else
            {
              log::INFO<log::FETCH>("<<< decodeAndPlay SUCCESS for segment [{}]", seg_idx);
            }

            ++seg_idx;
            cv.notify_one(); // Wake fetcher
          }
        });

      for (size_t i = 0; i < segment_list.size(); ++i)
      {
        const std::string& seg_name = segment_list[i];
        log::INFO<log::FETCH>(">>> Fetching segment [{}] - {}", i, seg_name);

        AudioData segment = fetch_single_segment(nickname, audio_id, seg_name, client.get());
        if (segment.empty())
        {
          log::WARN<log::FETCH>("!!! Failed to fetch segment [{}] - {}", i, seg_name);
          continue;
        }

        log::INFO<log::FETCH>("<<< Fetched segment [{}] - {} ({} bytes)", i, seg_name,
                              segment.size());

        std::unique_lock lock(mtx);
        cv.wait(lock, [&] { return !has_next; }); // Wait for playback to consume previous
        next_segment = std::move(segment);
        has_next     = true;
        lock.unlock();
        cv.notify_one(); // Wake playback
      }

      // End-of-stream signal
      {
        std::lock_guard lock(mtx);
        finished = true;
      }
      cv.notify_one(); // Wake playback one last time

      playback_thread.join();

      return true;
    }
    catch (std::exception& e)
    {
      log::ERROR<log::FETCH>("streamAndPlay threw error: {}", e.what());
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

    log::TRACE<log::FETCH>("Attempting to fetch client list of owner {} through Wavy-Server at {}",
                           targetNickname, server);

    NetResponse response = client.get(macros::to_string(macros::SERVER_PATH_HLS_OWNERS));

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
  std::shared_ptr<asio::io_context> ioc_;
  std::shared_ptr<ssl::context>     ctx_;
  IPAddr                            server_;

  auto make_client() -> std::unique_ptr<libwavy::network::HttpsClient>
  {
    return std::make_unique<libwavy::network::HttpsClient>(*ioc_, *ctx_, server_);
  }

  auto fetch_master_playlist(const StorageOwnerID& nickname, const StorageAudioID& audio_id)
    -> PlaylistData
  {
    const NetTarget masterPlaylistPath =
      "/hls/" + nickname + "/" + audio_id + "/" + macros::to_string(macros::MASTER_PLAYLIST);

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

    const NetTarget playlist_path = "/hls/" + nickname + "/" + audio_id + "/" + selected;
    log::INFO<log::FETCH>("Selected bitrate playlist: {}", playlist_path);

    auto client = make_client();
    return client->get(playlist_path);
  }

  auto contains_m4s(const std::vector<std::string>& segs) -> bool
  {
    return std::ranges::any_of(segs.begin(), segs.end(),
                               [](const auto& s) { return s.ends_with(macros::M4S_FILE_EXT); });
  }

  auto fetch_single_segment(const StorageOwnerID& nickname, const StorageAudioID& audio_id,
                            const std::string& seg, libwavy::network::HttpsClient* client)
    -> AudioData
  {
    const NetTarget url = "/hls/" + nickname + "/" + audio_id + "/" + seg;
    log::TRACE<log::FETCH>("Streaming segment: {}", url);
    return client->get(url);
  }

  auto parse_segment_list(const std::string& playlist) -> std::vector<std::string>
  {
    std::istringstream       stream(playlist);
    std::string              line;
    std::vector<std::string> segments;
    while (std::getline(stream, line))
    {
      if (!line.empty() && line[0] != '#')
        segments.push_back(std::move(line));
    }
    return segments;
  }
};

} // namespace libwavy::fetch
