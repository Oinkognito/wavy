#if __cplusplus < 202002L
#error "Wavy-Client requires C++20 or later."
#endif

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

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

std::string perform_https_request(net::io_context& ioc, ssl::context& ctx,
                                  const std::string& target)
{
  // Resolve the server address
  tcp::resolver resolver(ioc);
  auto const    results = resolver.resolve("localhost", "8080");

  // Create SSL stream
  beast::ssl_stream<tcp::socket> stream(ioc, ctx);

  // Connect and perform SSL handshake
  net::connect(stream.next_layer(), results.begin(), results.end());
  stream.handshake(ssl::stream_base::client);

  // Create and send HTTP GET request
  http::request<http::string_body> req{http::verb::get, target, 11};
  req.set(http::field::host, "localhost");
  req.set(http::field::user_agent, "WavyClient");
  http::write(stream, req);

  // Read response
  beast::flat_buffer                 buffer;
  http::response<http::dynamic_body> res;
  http::read(stream, buffer, res);

  // Get response data before shutdown
  std::string response_data = boost::beast::buffers_to_string(res.body().data());

  // Attempt shutdown but ignore SSL_ERROR_STREAM_TRUNCATED
  beast::error_code ec;
  stream.shutdown(ec);

  return response_data;
}

int main()
{
  try
  {
    // Initialize SSL context
    net::io_context ioc;
    ssl::context    ctx(ssl::context::tlsv12_client);
    ctx.set_verify_mode(ssl::verify_none); // Disable verification for testing

    // Get list of clients
    // std::string clients_response = perform_https_request(ioc, ctx, "/hls/clients");
    // std::cout << "Available clients:\n" << clients_response << "\n";

    // // Get user input
    // std::cout << "Enter client ID: ";
    std::string client_id = "a5d31814-ff1b-43da-bf6c-1fe6c1b95898";
    // std::getline(std::cin, client_id);

    // if (client_id.empty())
    // {
    //   std::cerr << "Error: Client ID cannot be empty\n";
    //   return EXIT_FAILURE;
    // }

    // Request the main index playlist
    std::string index_playlist_path = "/hls/" + client_id + "/index.m3u8";
    std::cout << "Requesting index playlist: " << index_playlist_path << "\n";
    std::string playlist_content = perform_https_request(ioc, ctx, index_playlist_path);

    // Check if the index playlist contains multiple m3u8 streams
    if (playlist_content.find("#EXT-X-STREAM-INF:") != std::string::npos)
    {
      int                max_bandwidth = 0;
      std::string        selected_playlist;
      std::istringstream iss(playlist_content);
      std::string        line;
      while (std::getline(iss, line))
      {
        // Look for stream info lines
        if (line.find("#EXT-X-STREAM-INF:") != std::string::npos)
        {
          size_t pos = line.find("BANDWIDTH=");
          if (pos != std::string::npos)
          {
            pos += 10; // length of "BANDWIDTH="
            // Extract bandwidth value until the next comma or end of line.
            size_t      endPos       = line.find_first_of(", ", pos);
            std::string bandwidthStr = line.substr(pos, endPos - pos);
            int         bandwidth    = std::stoi(bandwidthStr);

            // The next line should contain the relative m3u8 file name.
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
        return EXIT_FAILURE;
      }
      // Request the highest bitrate playlist
      std::string highest_playlist_path = "/hls/" + client_id + "/" + selected_playlist;
      std::cout << "Requesting highest bitrate playlist: " << highest_playlist_path << "\n";
      playlist_content = perform_https_request(ioc, ctx, highest_playlist_path);
    }

    // At this point, playlist_content should be a list of transport segments.
    std::istringstream segment_stream(playlist_content);
    std::string        segmentLine;
    while (std::getline(segment_stream, segmentLine))
    {
      // Skip empty lines and comments
      if (segmentLine.empty() || segmentLine[0] == '#')
      {
        continue;
      }
      // Log the segment request
      std::cout << "Streaming segment: " << segmentLine << "\n";

      // Request the transport stream segment and write it to standard output.
      std::string segment_data =
        perform_https_request(ioc, ctx, "/hls/" + client_id + "/" + segmentLine);
      std::cout.write(segment_data.data(), segment_data.size());
      std::cout.flush();
    }
    return EXIT_SUCCESS;
  }
  catch (const std::exception& e)
  {
    std::cerr << "Error: " << e.what() << "\n";
    return EXIT_FAILURE;
  }
}
