#pragma once

#include <cstdlib>
#include <sstream>
#include <string>
#include <vector>

#include <libwavy/common/macros.hpp>
#include <libwavy/common/state.hpp>
#include <libwavy/ffmpeg/decoder/entry.hpp>
#include <libwavy/logger.hpp>
#include <libwavy/network/entry.hpp>

namespace libwavy::fetch
{

class TSegFetcher
{
public:
  TSegFetcher(const std::string& server)
      : ioc_(), ctx_(ssl::context::tlsv12_client), client_(ioc_, ctx_, server)
  {
    ctx_.set_verify_mode(ssl::verify_none);
  }

  auto fetch(const std::string& ip_id, const std::string& audio_id, GlobalState& gs,
             int desired_bandwidth, bool& flac_found) -> bool
  {
    LOG_INFO << FETCH_LOG << "Request Owner: " << ip_id;
    LOG_INFO << FETCH_LOG << "Audio-ID: " << audio_id;
    LOG_INFO << FETCH_LOG << "Bitrate: " << desired_bandwidth;

    auto master_playlist = fetch_master_playlist(ip_id, audio_id);
    std::cout << master_playlist << std::endl;
    if (master_playlist.empty())
      return false;

    auto content = select_playlist(master_playlist, desired_bandwidth);
    if (content.empty())
      return false;

    std::string              init_mp4_data;
    std::vector<std::string> m4s_segments;
    if (!process_segments(content, ip_id, audio_id, init_mp4_data, m4s_segments, gs))
      return false;

    if (!m4s_segments.empty())
    {
      flac_found = true;
      gs.transport_segments.push_back(std::move(init_mp4_data));
      gs.transport_segments.insert(gs.transport_segments.end(),
                                   std::make_move_iterator(m4s_segments.begin()),
                                   std::make_move_iterator(m4s_segments.end()));
    }

    if (!DBG_WriteTransportSegmentsToFile(gs.transport_segments, "audio.raw"))
    {
      LOG_ERROR << FETCH_LOG << "Error writing transport segments to file";
      return false;
    }

    LOG_INFO << FETCH_LOG << "Stored " << gs.transport_segments.size() << " transport segments.";
    return true;
  }

private:
  asio::io_context              ioc_;
  ssl::context                  ctx_;
  libwavy::network::HttpsClient client_;

  auto fetch_master_playlist(const std::string& ip_id, const std::string& audio_id) -> std::string
  {
    std::string path =
      "/hls/" + ip_id + "/" + audio_id + "/" + macros::to_string(macros::MASTER_PLAYLIST);
    std::string content = client_.get(path);
    if (content.empty())
    {
      LOG_ERROR << FETCH_LOG << "Failed to fetch master playlist for " << ip_id << "/" << audio_id;
    }
    return content;
  }

  auto select_playlist(const std::string& content, int desired_bandwidth) -> std::string
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

    LOG_INFO << FETCH_LOG << "Selected bitrate playlist: " << selected;
    return client_.get(selected);
  }

  auto process_segments(const std::string& playlist, const std::string& ip_id,
                        const std::string& audio_id, std::string& init_mp4_data,
                        std::vector<std::string>& m4s_segments, GlobalState& gs) -> bool
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

    if (has_m4s)
    {
      init_mp4_data = client_.get("/hls/" + ip_id + "/" + audio_id + "/init.mp4");
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
        std::string url  = "/hls/" + ip_id + "/" + audio_id + "/" + line;
        std::string data = client_.get(url);

        if (!data.empty())
        {
          if (line.ends_with(macros::M4S_FILE_EXT))
            m4s_segments.push_back(std::move(data));
          else if (line.ends_with(macros::TRANSPORT_STREAM_EXT))
            gs.transport_segments.push_back(std::move(data));

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
