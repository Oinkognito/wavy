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

#include <iostream>
#include <libwavy/logger.hpp>
#include <libwavy/playback.hpp>
#include <libwavy/tsfetcher/entry.hpp>

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

auto decodeAndPlay(GlobalState& gs, bool& flac_found) -> bool
{
  if (gs.transport_segments.empty())
  {
    LOG_ERROR << DECODER_LOG << "No transport stream segments provided";
    return false;
  }

  LOG_INFO << "Decoding transport stream segments...";

  libwavy::ffmpeg::MediaDecoder decoder;
  std::vector<unsigned char>    decoded_audio;
  if (!decoder.decode(gs.transport_segments, decoded_audio))
  {
    LOG_ERROR << DECODER_LOG << "Decoding failed";
    return false;
  }

  try
  {
    LOG_INFO << RECEIVER_LOG << "Starting audio playback...";
    libwavy::audio::AudioPlayer player(decoded_audio, flac_found);
    player.play();
  }
  catch (const std::exception& e)
  {
    LOG_ERROR << AUDIO_LOG << "Audio playback error: " << e.what();
    return false;
  }

  return true;
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
    return WAVY_RET_FAIL;
  }

  const std::string ip_id   = argv[1];
  const int         index   = std::stoi(argv[2]);
  const std::string server  = argv[3];
  const int         bitrate = std::stoi(argv[4]);

  const std::vector<std::string> clients = fetch_client_list(server, ip_id);

  if (index < 0 || index >= static_cast<int>(clients.size()))
  {
    LOG_ERROR << RECEIVER_LOG << "Invalid index. Available range: 0 to " << clients.size() - 1;
    return WAVY_RET_FAIL;
  }

  std::string                 audio_id   = clients[index];
  bool                        flac_found = false;
  GlobalState                 gs;
  libwavy::fetch::TSegFetcher fetcher(ip_id);

  if (!fetcher.fetch(ip_id, audio_id, gs, bitrate, flac_found))
  {
    LOG_ERROR << RECEIVER_LOG << "Something went horribly wrong while fetching!!";
    return EXIT_FAILURE;
  }

  if (!decodeAndPlay(gs, flac_found))
  {
    return WAVY_RET_FAIL;
  }

  return WAVY_RET_SUC;
}
