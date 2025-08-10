#pragma once
/********************************************************************************
 *                                Wavy Project                                  *
 *                         High-Fidelity Audio Streaming                        *
 *                                                                              *
 *  Copyright (c) 2025 Oinkognito                                               *
 *  All rights reserved.                                                        *
 *                                                                              *
 *  License:                                                                    *
 *  This software is licensed under the BSD-3-Clause License. You may use,      *
 *  modify, and distribute this software under the conditions stated in the     *
 *  LICENSE file provided in the project root.                                  *
 *                                                                              *
 *  Warranty Disclaimer:                                                        *
 *  This software is provided "AS IS", without any warranties or guarantees,    *
 *  either expressed or implied, including but not limited to fitness for a     *
 *  particular purpose.                                                         *
 *                                                                              *
 *  Contributions:                                                              *
 *  Contributions are welcome. By submitting code, you agree to license your    *
 *  contributions under the same BSD-3-Clause terms.                            *
 *                                                                              *
 *  See LICENSE file for full legal details.                                    *
 ********************************************************************************/

#include <chrono>
#include <libwavy/server/owner-metrics.hpp>
#include <libwavy/utils/math/entry.hpp>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace libwavy::server
{

struct Metrics
{
  std::atomic<ui64> total_requests{0};
  std::atomic<ui64> successful_requests{0};
  std::atomic<ui64> failed_requests{0};
  std::atomic<ui64> upload_requests{0};
  std::atomic<ui64> delete_requests{0};
  std::atomic<ui64> download_requests{0};
  std::atomic<ui64> bytes_uploaded{0};
  std::atomic<ui64> bytes_downloaded{0};
  std::atomic<ui64> active_connections{0};
  std::atomic<ui64> total_connections{0};

  // Response time tracking
  mutable std::mutex                     response_times_mutex;
  std::vector<std::chrono::milliseconds> recent_response_times;
  static const size_t                    MAX_RESPONSE_TIMES = 1000;

  // Error tracking
  std::atomic<ui64> error_500_count{0};
  std::atomic<ui64> error_400_count{0};
  std::atomic<ui64> error_404_count{0};
  std::atomic<ui64> error_403_count{0};

  // key = owner nickname
  mutable std::shared_mutex                     owners_mutex;
  std::unordered_map<std::string, OwnerMetrics> owners;

  // Uptime tracking
  std::chrono::steady_clock::time_point start_time;

  Metrics() : start_time(std::chrono::steady_clock::now())
  {
    recent_response_times.reserve(MAX_RESPONSE_TIMES);
  }

  void record_owner_upload(const StorageOwnerID& owner_id, ui64 bytes)
  {
    std::unique_lock lock(owners_mutex);
    auto&            om = owners[owner_id];
    om.uploads++;
    om.storage_bytes += bytes;
  }

  void record_owner_delete(const StorageOwnerID& owner_id)
  {
    std::unique_lock lock(owners_mutex);
    auto&            om = owners[owner_id];
    om.deletes++;
    om.storage_bytes = 0;
  }

  auto get_top_owner_by_songs() const -> StorageOwnerID
  {
    std::shared_lock lock(owners_mutex);
    std::string      top_owner;
    ui64             max_songs = 0;

    for (const auto& [owner, metrics] : owners)
    {
      if (metrics.songs_count > max_songs)
      {
        max_songs = metrics.songs_count;
        top_owner = owner;
      }
    }
    return top_owner;
  }

  auto get_top_owner_by_storage() const -> StorageOwnerID
  {
    std::shared_lock lock(owners_mutex);
    std::string      top_owner;
    ui64             max_bytes = 0;

    for (const auto& [owner, metrics] : owners)
    {
      if (metrics.storage_bytes > max_bytes)
      {
        max_bytes = metrics.storage_bytes;
        top_owner = owner;
      }
    }
    return top_owner;
  }

  auto get_owner_metrics(const StorageOwnerID& owner_id)
    -> std::optional<std::reference_wrapper<const OwnerMetrics>>
  {
    auto it = owners.find(owner_id);
    if (it != owners.end())
    {
      return std::cref(it->second);
    }
    return std::nullopt;
  }

  void record_response_time(std::chrono::milliseconds duration)
  {
    std::lock_guard<std::mutex> lock(response_times_mutex);
    recent_response_times.push_back(duration);
    if (recent_response_times.size() > MAX_RESPONSE_TIMES)
    {
      recent_response_times.erase(recent_response_times.begin());
    }
  }

  [[nodiscard]] auto get_avg_response_time() const -> double
  {
    std::lock_guard<std::mutex> lock(response_times_mutex);
    if (recent_response_times.empty())
      return 0.0;

    auto total = std::chrono::milliseconds(0);
    for (const auto& time : recent_response_times)
    {
      total += time;
    }
    return static_cast<double>(total.count()) / recent_response_times.size();
  }

  [[nodiscard]] auto get_uptime() const -> std::chrono::seconds
  {
    return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() -
                                                            start_time);
  }
};

class MetricsSerializer
{
public:
  static auto to_prometheus_format(const Metrics& m) -> std::string
  {
    std::ostringstream out;

    // Helper lambda to reduce duplication
    auto metric = [&](const std::string& name, const std::string& type, const std::string& help,
                      const std::atomic<ui64>& value)
    {
      out << "# HELP " << name << " " << help << "\n";
      out << "# TYPE " << name << " " << type << "\n";
      out << name << " " << value << "\n\n";
    };

    metric("wavy_requests_total", "counter", "Total number of HTTP requests", m.total_requests);
    metric("wavy_requests_successful", "counter", "Total successful requests",
           m.successful_requests);
    metric("wavy_requests_failed", "counter", "Total failed requests", m.failed_requests);
    metric("wavy_delete_requests", "counter", "Total DELETE requests", m.delete_requests);

    metric("wavy_active_connections", "gauge", "Current active connections", m.active_connections);
    metric("wavy_response_time_avg", "gauge", "Average response time in milliseconds",
           m.get_avg_response_time());

    metric("wavy_uptime_seconds", "gauge", "Server uptime in seconds", m.get_uptime().count());

    metric("wavy_bytes_uploaded_total", "counter", "Total bytes uploaded", m.bytes_uploaded);
    metric("wavy_bytes_downloaded_total", "counter", "Total bytes downloaded", m.bytes_downloaded);

    return out.str();
  }

  static auto owner_to_prometheus_format(const StorageOwnerID& owner_id, const OwnerMetrics& om)
    -> std::string
  {
    std::ostringstream out;

    auto metric = [&](const std::string& name, const std::string& help, ui64 value)
    {
      out << "# HELP " << name << " " << help << "\n";
      out << "# TYPE " << name << " counter\n";
      out << name << "{owner=\"" << owner_id << "\"} " << value << "\n\n";
    };

    metric("wavy_owner_uploads_total", "Total uploads from this owner", om.uploads);
    metric("wavy_owner_deletes_total", "Total deletes from this owner", om.deletes);
    metric("wavy_owner_songs_count", "Current songs count for this owner", om.songs_count);
    metric("wavy_owner_storage_bytes", "Total storage used by this owner", om.storage_bytes);

    return out.str();
  }
};

} // namespace libwavy::server
