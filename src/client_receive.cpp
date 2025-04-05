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

#if __cplusplus < 202002L
#error "Wavy-Client requires C++20 or later."
#endif

#include <cstdlib>
#include <sstream>
#include <string>
#include <vector>

#include <libwavy/common/macros.hpp>
#include <libwavy/common/state.hpp>
#include <libwavy/ffmpeg/decoder/entry.hpp>
#include <libwavy/logger.hpp>
#include <libwavy/network/entry.hpp>
#include <libwavy/playback.hpp>

auto fetch_transport_segments(const std::string& ip_id, const std::string& audio_id,
                              GlobalState& gs, const std::string& server,
                              const int desired_bandwith, bool& flac_found) -> bool
{
  asio::io_context ioc; // boost::asio handles async network contexts as well
  ssl::context     ctx(ssl::context::tlsv12_client);
  ctx.set_verify_mode(ssl::verify_none);
  libwavy::network::HttpsClient client(ioc, ctx, server);

  LOG_INFO << RECEIVER_LOG << "Request Owner: " << ip_id << " for audio-id: " << audio_id;

  std::string playlist_path =
    "/hls/" + ip_id + "/" + audio_id + "/" + macros::to_string(macros::MASTER_PLAYLIST);
  std::string playlist_content = client.get(playlist_path);

  if (playlist_content.empty())
  {
    LOG_ERROR << RECEIVER_LOG << "Failed to fetch playlist for " << ip_id << "/" << audio_id;
    return false;
  }

  if (playlist_content.find(macros::PLAYLIST_VARIANT_TAG) != std::string::npos)
  {
    std::string        selected_playlist;
    std::istringstream iss(playlist_content);
    std::string        line;

    while (std::getline(iss, line))
    {
      if (line.find(macros::PLAYLIST_VARIANT_TAG) != std::string::npos)
      {
        size_t pos = line.find("BANDWIDTH=");
        if (pos != std::string::npos)
        {
          pos += 10;
          size_t      endPos       = line.find_first_of(", ", pos);
          std::string bandwidthStr = line.substr(pos, endPos - pos);
          int         bandwidth    = std::stoi(bandwidthStr);

          std::string streamPlaylist;
          if (std::getline(iss, streamPlaylist))
          {
            if (bandwidth == desired_bandwith)
            {
              selected_playlist = streamPlaylist;
            }
          }
        }
      }
    }

    if (selected_playlist.empty())
    {
      LOG_ERROR << RECEIVER_LOG << "Could not find a valid stream playlist";
      return false;
    }

    LOG_INFO << RECEIVER_LOG << "Selected highest bitrate playlist: " << selected_playlist;

    playlist_path    = "/hls/" + ip_id + "/" + audio_id + "/" + selected_playlist;
    playlist_content = client.get(playlist_path);
  }

  std::istringstream       segment_stream(playlist_content);
  std::string              line;
  std::string              init_mp4_data;
  bool                     has_m4s_segments = false;
  std::vector<std::string> m4s_segments;

  while (std::getline(segment_stream, line))
  {
    if (!line.empty() && line[0] != '#')
    {
      if (line.ends_with(macros::M4S_FILE_EXT))
      {
        has_m4s_segments = true;
      }
    }
  }

  if (has_m4s_segments)
  {
    std::string init_mp4_url = "/hls/" + ip_id + "/" + audio_id + "/init.mp4";
    init_mp4_data            = client.get(init_mp4_url);

    if (init_mp4_data.empty())
    {
      LOG_ERROR << RECEIVER_LOG << "Failed to fetch init.mp4 for " << ip_id << "/" << audio_id;
      return false;
    }

    LOG_INFO << RECEIVER_LOG << "Fetched init.mp4, size: " << init_mp4_data.size() << " bytes.";
    flac_found = true;
  }

  // Re-parse playlist for segments
  segment_stream.clear();
  segment_stream.seekg(0, std::ios::beg);

  while (std::getline(segment_stream, line))
  {
    if (!line.empty() && line[0] != '#')
    {
      std::string segment_url = "/hls/" + ip_id + "/" + audio_id + "/" + line;

      if (line.ends_with(macros::TRANSPORT_STREAM_EXT) || line.ends_with(macros::M4S_FILE_EXT))
      {
        std::string segment_data = client.get(segment_url);
        if (!segment_data.empty())
        {
          if (line.ends_with(macros::M4S_FILE_EXT))
          {
            m4s_segments.push_back(std::move(segment_data));
          }
          else
          {
            gs.transport_segments.push_back(std::move(segment_data));
          }

          LOG_DEBUG << RECEIVER_LOG << "Fetched segment: " << line;
        }
        else
        {
          LOG_WARNING << RECEIVER_LOG << "Failed to fetch segment: " << line;
        }
      }
    }
  }

  // Prepend init.mp4 ONCE before all .m4s segments
  if (!m4s_segments.empty())
  {
    gs.transport_segments.push_back(std::move(init_mp4_data));
    gs.transport_segments.insert(gs.transport_segments.end(),
                                 std::make_move_iterator(m4s_segments.begin()),
                                 std::make_move_iterator(m4s_segments.end()));
  }
  /************************************************************************************
   *
   * @NOTE
   *
   * To check the final file received from the server.
   *
   * if (!write_transport_segments_to_file(gs.transport_segments, "audio.raw"))
   * {
   *  LOG_ERROR << "Error writing transport segments to file";
   *  return false;
   * }
   ***********************************************************************************/

  if (!DBG_WriteTransportSegmentsToFile(gs.transport_segments, "audio.raw"))
  {
    LOG_ERROR << "Error writing transport segments to file";
    return false;
  }

  LOG_INFO << "Stored " << gs.transport_segments.size() << " transport segments.";
  return true;
}

auto decode_and_play(GlobalState& gs, bool& flac_found) -> bool
{
  if (gs.transport_segments.empty())
  {
    LOG_ERROR << "No transport stream segments provided";
    return false;
  }

  LOG_INFO << "Decoding transport stream segments...";

  libwavy::ffmpeg::MediaDecoder decoder;
  std::vector<unsigned char>    decoded_audio;
  if (!decoder.decode(gs.transport_segments, decoded_audio))
  {
    LOG_ERROR << "Decoding failed";
    return false;
  }

  try
  {
    LOG_INFO << "Starting audio playback...";
    AudioPlayer player(decoded_audio, flac_found);
    player.play();
  }
  catch (const std::exception& e)
  {
    LOG_ERROR << "Audio playback error: " << e.what();
    return false;
  }

  return true;
}

auto fetch_client_list(const std::string& server, const std::string& target_ip_id)
  -> std::vector<std::string>
{
  asio::io_context ioc;
  ssl::context     ctx(ssl::context::tlsv12_client);
  ctx.set_verify_mode(ssl::verify_none);
  libwavy::network::HttpsClient client(ioc, ctx, server);

  std::string response = client.get(macros::to_string(macros::SERVER_PATH_HLS_CLIENTS));

  std::istringstream       iss(response);
  std::string              line;
  std::string              current_ip_id;
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
        std::string client_id = line.substr(pos + 2); // Extract client ID
        clients.push_back(client_id);
      }
    }
  }

  return clients;
}

void print_client_list(const std::vector<std::string>& clients)
{
  if (clients.empty())
  {
    std::cout << "No clients found.\n";
    return;
  }

  std::cout << "Available Clients:\n";
  for (size_t i = 0; i < clients.size(); ++i)
  {
    std::cout << "  [" << i << "] " << clients[i] << "\n";
  }
}

auto main(int argc, char* argv[]) -> int
{
  libwavy::log::init_logging();

  if (argc < 5)
  {
    LOG_ERROR << "Usage: " << argv[0] << " <ip-id> <index> <server-ip> <bitrate-stream>";
    return EXIT_FAILURE;
  }

  std::string ip_id  = argv[1];
  int         index  = std::stoi(argv[2]);
  std::string server = argv[3];

  std::vector<std::string> clients = fetch_client_list(server, ip_id);

  if (index < 0 || index >= static_cast<int>(clients.size()))
  {
    LOG_ERROR << "Invalid index. Available range: 0 to " << clients.size() - 1;
    return EXIT_FAILURE;
  }

  std::string audio_id   = clients[index];
  bool        flac_found = false;
  int         bitrate    = std::stoi(argv[4]);
  GlobalState gs;

  if (!fetch_transport_segments(ip_id, audio_id, gs, server, bitrate, flac_found))
  {
    return EXIT_FAILURE;
  }

  if (!decode_and_play(gs, flac_found))
  {
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
