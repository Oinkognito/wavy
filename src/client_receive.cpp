#if __cplusplus < 202002L
#error "Wavy-Client requires C++20 or later."
#endif

#include <cstdlib>
#include <sstream>
#include <string>
#include <vector>

#include "../include/decode.hpp"
#include "../include/logger.hpp"
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
namespace http  = beast::http;
namespace net   = boost::asio;
using tcp       = net::ip::tcp;

// Perform an HTTPS GET request and return the response body as a string.
auto perform_https_request(net::io_context& ioc, ssl::context& ctx, const std::string& target,
                           const std::string& server) -> std::string
{
  try
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

    if (ec == net::error::eof)
    {
      ec.clear();
    }
    else if (ec)
    {
      LOG_WARNING << "Stream shutdown error: " << ec.message();
    }

    return response_data;
  }
  catch (const std::exception& e)
  {
    LOG_ERROR << "HTTPS request failed: " << e.what();
    return "";
  }
}

auto fetch_transport_segments(int index, GlobalState& gs, const std::string& server) -> bool
{
  net::io_context ioc;
  ssl::context    ctx(ssl::context::tlsv12_client);
  ctx.set_verify_mode(ssl::verify_none);

  LOG_INFO << "Fetching client list from server: " << server;

  std::string        clients_response = perform_https_request(ioc, ctx, "/hls/clients", server);
  std::istringstream clientStream(clients_response);
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
    LOG_ERROR << "Invalid client index: " << index;
    return false;
  }

  std::string client_id = clientIds[index];
  if (client_id.empty())
  {
    LOG_ERROR << "Client ID cannot be empty";
    return false;
  }

  LOG_INFO << "Selected client ID: " << client_id;

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
      LOG_ERROR << "Could not find a valid stream playlist";
      return false;
    }

    LOG_INFO << "Selected highest bitrate playlist: " << selected_playlist;

    std::string highest_playlist_path = "/hls/" + client_id + "/" + selected_playlist;
    playlist_content = perform_https_request(ioc, ctx, highest_playlist_path, server);
  }

  std::istringstream segment_stream(playlist_content);
  while (std::getline(segment_stream, line))
  {
    if (!line.empty() && line[0] != '#')
    {
      std::string segment_data =
        perform_https_request(ioc, ctx, "/hls/" + client_id + "/" + line, server);
      gs.transport_segments.push_back(std::move(segment_data));
      LOG_DEBUG << "Fetched segment: " << line;
    }
  }

  LOG_INFO << "Stored " << gs.transport_segments.size() << " transport segments.";
  return true;
}

auto decode_and_play(GlobalState& gs) -> bool
{
  if (gs.transport_segments.empty())
  {
    LOG_ERROR << "No transport stream segments provided";
    return false;
  }

  LOG_INFO << "Decoding transport stream segments...";

  TSDecoder                  decoder;
  std::vector<unsigned char> decoded_audio;
  if (!decoder.decode_ts(gs.transport_segments, decoded_audio))
  {
    LOG_ERROR << "Decoding failed";
    return false;
  }

  try
  {
    LOG_INFO << "Starting audio playback...";
    AudioPlayer player(decoded_audio);
    player.play();
  }
  catch (const std::exception& e)
  {
    LOG_ERROR << "Audio playback error: " << e.what();
    return false;
  }

  return true;
}

auto main(int argc, char* argv[]) -> int
{
  logger::init_logging();

  if (argc < 3)
  {
    LOG_ERROR << "Usage: " << argv[0] << " <client-index> <server-ip>";
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
