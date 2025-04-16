#pragma once

#include <libwavy/tsfetcher/plugin/entry.hpp>
#include <libwavy/utils/audio/entry.hpp>

namespace libwavy::components::client
{
class WavyClient
{
private:
  std::string _ip_id;
  std::string _server;
  std::string _plugin_path;
  GlobalState gs;
  int         _bitrate;

public:
  WavyClient(const std::string ip_id, const std::string server, const std::string plugin_path,
             const int bitrate)
      : _ip_id(ip_id), _server(server), _plugin_path(plugin_path), _bitrate(bitrate)
  {
  }

  auto start(bool flac_found, int index) -> int
  {
    LOG_INFO << "Starting Client Fetch and Playback Daemon...";

    std::unique_ptr<libwavy::fetch::ISegmentFetcher,
                    std::function<void(libwavy::fetch::ISegmentFetcher*)>>
      fetcher;

    try
    {
      // Attempt to load the plugin dynamically based on the path
      fetcher = libwavy::fetch::plugin::FetcherFactory::create(_plugin_path, _ip_id);
    }
    catch (const std::exception& e)
    {
      LOG_ERROR << PLUGIN_LOG << "Plugin error: " << e.what();
      return WAVY_RET_FAIL;
    }

    // Fetch client list and audio ID
    const std::vector<std::string> clients = fetcher->fetch_client_list(_server, _ip_id);
    if (clients.empty())
    {
      LOG_ERROR << "Failed to fetch clients. Exiting...";
      return WAVY_RET_FAIL;
    }
    std::string audio_id = clients[index];

    // Validate the index
    if (index < 0 || index >= static_cast<int>(clients.size()))
    {
      LOG_ERROR << RECEIVER_LOG << "Invalid index. Available range: 0 to " << clients.size() - 1;
      return WAVY_RET_FAIL;
    }

    // Fetch the transport stream
    if (!fetcher->fetch(_ip_id, audio_id, gs, _bitrate, flac_found))
    {
      LOG_ERROR << RECEIVER_LOG << "Something went horribly wrong while fetching!!";
      return WAVY_RET_FAIL;
    }

    // Decode and play the fetched stream
    if (!decodeAndPlay(gs, flac_found))
    {
      return WAVY_RET_FAIL;
    }

    return WAVY_RET_SUC;
  }
};
} // namespace libwavy::components::client
