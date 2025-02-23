#pragma once

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast.hpp>
#include <iostream>
#include <map>
#include <sstream>
#include <string>

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
namespace ssl = boost::asio::ssl;
using tcp = net::ip::tcp;

class PlaylistParser {
public:
    PlaylistParser(net::io_context &ioc, ssl::context &ssl_ctx, const std::string &url)
        : resolver_(ioc), stream_(ioc, ssl_ctx), master_url_(url) {}

    bool fetchMasterPlaylist() {
        std::string host, port, target;
        parseUrl(master_url_, host, port, target);

        std::cout << "[INFO] Fetching master playlist from: " << master_url_ << "\n";

        try {
            std::cout << "[INFO] Resolving host: " << host << " on port " << port << "\n";
            auto const results = resolver_.resolve(host, port);

            std::cout << "[INFO] Connecting to: " << host << ":" << port << "\n";
            beast::get_lowest_layer(stream_).connect(results);
            stream_.handshake(ssl::stream_base::client);

            http::request<http::string_body> req{http::verb::get, target, 11};
            req.set(http::field::host, host);
            req.set(http::field::user_agent, "Boost.Beast");

            std::cout << "[INFO] Sending HTTP GET request...\n";
            http::write(stream_, req);

            beast::flat_buffer buffer;
            http::response<http::dynamic_body> res;
            http::read(stream_, buffer, res);

            std::cout << "[INFO] Received HTTP Response. Status: " << res.result_int() << "\n";
            beast::error_code ec;
            stream_.shutdown(ec);
            if (ec && ec != beast::errc::not_connected) {
                std::cerr << "[WARN] SSL Shutdown failed: " << ec.message() << "\n";
            }

            std::string body = beast::buffers_to_string(res.body().data());

            parsePlaylist(body);
            return true;
        } catch (const std::exception &e) {
            std::cerr << "[ERROR] Error fetching master playlist: " << e.what() << "\n";
            return false;
        }
    }

    std::map<int, std::string> getBitratePlaylists() const {
        return bitrate_playlists_;
    }

private:
    net::ip::tcp::resolver resolver_;
    ssl::stream<beast::tcp_stream> stream_;
    std::string master_url_;
    std::map<int, std::string> bitrate_playlists_;

    void parseUrl(const std::string &url, std::string &host, std::string &port, std::string &target) {
        size_t pos = url.find("//");
        size_t start = (pos == std::string::npos) ? 0 : pos + 2;
        size_t end = url.find('/', start);

        std::string full_host = url.substr(start, end - start);
        size_t port_pos = full_host.find(':');

        if (port_pos != std::string::npos) {
            host = full_host.substr(0, port_pos);
            port = full_host.substr(port_pos + 1);
        } else {
            host = full_host;
            port = "443"; // Default HTTPS port
        }

        target = (end == std::string::npos) ? "/" : url.substr(end);
        std::cout << "[DEBUG] Parsed Host: " << host << ", Port: " << port << ", Target: " << target << "\n";
    }

    void parsePlaylist(const std::string &playlist) {
        std::istringstream ss(playlist);
        std::string line;
        int current_bitrate = 0;

        while (std::getline(ss, line)) {
            if (line.find("#EXT-X-STREAM-INF") != std::string::npos) {
                size_t bitrate_pos = line.find("BANDWIDTH=");
                if (bitrate_pos != std::string::npos) {
                    size_t start = bitrate_pos + 9;  // Position after "BANDWIDTH="
                    size_t end = line.find(',', start); // Find next comma
                    
                    std::string bitrate_str = line.substr(start, end - start);
                    
                    // Remove any '=' sign if it appears (edge case)
                    bitrate_str.erase(std::remove(bitrate_str.begin(), bitrate_str.end(), '='), bitrate_str.end());

                    std::cout << "[DEBUG] Extracted Bitrate String: '" << bitrate_str << "'\n";

                    // Ensure it's numeric
                    if (!bitrate_str.empty() && std::all_of(bitrate_str.begin(), bitrate_str.end(), ::isdigit)) {
                        try {
                            current_bitrate = std::stoi(bitrate_str);
                            std::cout << "[INFO] Parsed Bitrate: " << current_bitrate << " kbps\n";
                        } catch (const std::exception &e) {
                            std::cerr << "[ERROR] Failed to parse bitrate: " << e.what() << "\n";
                            continue;
                        }
                    } else {
                        std::cerr << "[ERROR] Invalid bitrate format: '" << bitrate_str << "'\n";
                        continue;
                    }
                }
            } else if (!line.empty() && line[0] != '#') {
                if (current_bitrate > 0) {
                    bitrate_playlists_[current_bitrate] = line;
                    std::cout << "[INFO] Added bitrate playlist: " << current_bitrate << " -> " << line << "\n";
                } else {
                    std::cerr << "[ERROR] Playlist URL found but no valid bitrate!\n";
                }
            }
        }
    }
};
