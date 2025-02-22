#if __cplusplus < 202002L
#error "Wavy-Client requires C++20 or later."
#endif

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "../include/decode.hpp"
#include "../include/macros.hpp"
#include "../include/playback.hpp"
#include "../include/state.hpp"
#include <boost/asio.hpp>
#include <boost/asio/ssl/error.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>

namespace ssl   = boost::asio::ssl;
namespace beast = boost::beast;
namespace http = beast::http;
namespace net  = boost::asio;
using tcp      = net::ip::tcp;

// Perform an HTTPS GET request and return the response body as a string.
auto perform_https_request(net::io_context& ioc, ssl::context& ctx, const std::string& target,
                           const std::string& server) -> std::string
{
  tcp::resolver resolver(ioc);
  auto const    results = resolver.resolve(server, WAVY_SERVER_PORT_NO_STR);

  beast::ssl_stream<tcp::socket> stream(ioc, ctx);
  net::connect(stream.next_layer(), results.begin(), results.end());
  stream.handshake(ssl::stream_base::client);

  http::request<http::string_body> req{http::verb::get, target, 11};
  req.set(http::field::host, server);
  req.set(http::field::user_agent, "WavyClient");
  http::write(stream, req);

  beast::flat_buffer                 buffer;
  http::response<http::dynamic_body> res;
  http::read(stream, buffer, res);

  std::string response_data = boost::beast::buffers_to_string(res.body().data());

  beast::error_code ec;
  stream.shutdown(ec);

  return response_data;
}

auto fetch_transport_segments(int index, GlobalState& gs, const std::string& server) -> bool
{
  net::io_context ioc;
  ssl::context    ctx(ssl::context::tlsv12_client);
  ctx.set_verify_mode(ssl::verify_none);

  std::string              clients_response = perform_https_request(ioc, ctx, "/hls/clients", server);
  std::istringstream       clientStream(clients_response);
  std::vector<std::string> clientIds;
  std::string              line;

  while (std::getline(clientStream, line))
  {
    if (!line.empty())
    {
      clientIds.push_back(line);
    }
  }

  if (index < 0 || index >= static_cast<int>(clientIds.size()))
  {
    std::cerr << "Error: Invalid client index.\n";
    return false;
  }

  std::string client_id = clientIds[index];
  if (client_id.empty())
  {
    std::cerr << "Error: Client ID cannot be empty\n";
    return false;
  }

  std::string index_playlist_path = "/hls/" + client_id + "/index.m3u8";
  std::string playlist_content    = perform_https_request(ioc, ctx, index_playlist_path, server);

  if (playlist_content.find("#EXT-X-STREAM-INF:") != std::string::npos)
  {
    int                max_bandwidth = 0;
    std::string        selected_playlist;
    std::istringstream iss(playlist_content);

    while (std::getline(iss, line))
    {
      if (line.find("#EXT-X-STREAM-INF:") != std::string::npos)
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
            if (bandwidth > max_bandwidth)
            {
              max_bandwidth     = bandwidth;
              selected_playlist = streamPlaylist;
            }
          }
        }
      }
    }

    if (selected_playlist.empty())
    {
      std::cerr << "Error: Could not find a valid stream playlist\n";
      return false;
    }

    std::string highest_playlist_path = "/hls/" + client_id + "/" + selected_playlist;
    playlist_content                  = perform_https_request(ioc, ctx, highest_playlist_path, server);
  }

  std::istringstream segment_stream(playlist_content);
  while (std::getline(segment_stream, line))
  {
    if (!line.empty() && line[0] != '#')
    {
      std::string segment_data = perform_https_request(ioc, ctx, "/hls/" + client_id + "/" + line, server);
      gs.transport_segments.push_back(std::move(segment_data));
    }
  }

  std::cout << "Stored " << gs.transport_segments.size() << " transport segments.\n";
  return true;
}

auto decode_and_play(GlobalState& gs) -> bool
{
  if (gs.transport_segments.empty())
  {
    av_log(nullptr, AV_LOG_ERROR, "No transport stream segments provided\n");
    return false;
  }

  TSDecoder                  decoder;
  std::vector<unsigned char> decoded_audio;
  if (!decoder.decode_ts(gs.transport_segments, decoded_audio))
  {
    av_log(nullptr, AV_LOG_ERROR, "Decoding failed\n");
    return false;
  }

  try
  {
    AudioPlayer player(decoded_audio);
    player.play();
  }
  catch (const std::exception& e)
  {
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
  }

  return true;
}

auto main(int argc, char* argv[]) -> int
{
  if (argc < 3)
  {
    std::cerr << "Usage: " << argv[0] << " <client-index> <server-ip>\n";
    return EXIT_FAILURE;
  }

  int         index = std::stoi(argv[1]);
  GlobalState gs;

  if (!fetch_transport_segments(index, gs, argv[2]))
  {
    return EXIT_FAILURE;
  }

  if (!decode_and_play(gs))
  {
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
