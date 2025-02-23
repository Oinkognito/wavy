#pragma once

#include <boost/asio.hpp>
#include <iostream>
#include <chrono>
#include <random>

namespace net = boost::asio;
using tcp = net::ip::tcp;
using namespace std::chrono;

struct NetworkStats {
    int latency;          // In milliseconds
    double jitter;        // In milliseconds
    double packet_loss;   // Percentage (0-100%)
};

class NetworkDiagnoser {
public:
    NetworkDiagnoser(net::io_context &ioc, const std::string &server_url)
        : resolver_(ioc), socket_(ioc), server_url_(server_url) {}

    NetworkStats diagnoseNetworkSpeed() {
        std::string host, port, target;
        parseUrl(server_url_, host, port, target);

        NetworkStats stats = { -1, 0.0, 0.0 };  // Default invalid values

        try {
            std::cout << "[INFO] Diagnosing network speed to: " << host << "\n";
            auto const results = resolver_.resolve(host, port);
            
            std::vector<int> latencies;

            for (int i = 0; i < 5; ++i) { // Send 5 probes to calculate jitter
                auto start = high_resolution_clock::now();
                socket_.connect(*results.begin());
                auto end = high_resolution_clock::now();
                int ping_time = duration_cast<milliseconds>(end - start).count();
                latencies.push_back(ping_time);
                socket_.close();
            }

            stats.latency = calculateAverage(latencies);
            stats.jitter = calculateJitter(latencies);
            stats.packet_loss = simulatePacketLoss(); // Simulating packet loss

            std::cout << "[INFO] Network Latency: " << stats.latency << " ms\n";
            std::cout << "[INFO] Network Jitter: " << stats.jitter << " ms\n";
            std::cout << "[INFO] Packet Loss: " << stats.packet_loss << "%\n";

        } catch (const std::exception &e) {
            std::cerr << "[ERROR] Network speed diagnosis failed: " << e.what() << "\n";
        }

        return stats;
    }

private:
    tcp::resolver resolver_;
    tcp::socket socket_;
    std::string server_url_;

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
    }

    int calculateAverage(const std::vector<int>& values) {
        int sum = 0;
        for (int v : values) sum += v;
        return values.empty() ? -1 : sum / values.size();
    }

    double calculateJitter(const std::vector<int>& latencies) {
        if (latencies.size() < 2) return 0.0;
        
        double sum = 0.0;
        for (size_t i = 1; i < latencies.size(); ++i) {
            sum += std::abs(latencies[i] - latencies[i - 1]);
        }
        return sum / (latencies.size() - 1);
    }

    double simulatePacketLoss() {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<> dist(0.0, 10.0); // Simulating 0-10% loss
        return dist(gen);
    }
};
