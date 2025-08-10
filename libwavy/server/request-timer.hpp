#pragma once

#include <libwavy/server/metrics.hpp>

namespace libwavy::server
{

// Request timing middleware
class RequestTimer
{
public:
  RequestTimer(Metrics& metrics) : metrics_(metrics), start_time_(std::chrono::steady_clock::now())
  {
    metrics_.total_requests++;
    metrics_.active_connections++;
  }

  ~RequestTimer()
  {
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now() - start_time_);
    metrics_.record_response_time(duration);
    metrics_.active_connections--;
  }

  void mark_success() { metrics_.successful_requests++; }
  void mark_failure() { metrics_.failed_requests++; }
  void mark_error_400() { metrics_.error_400_count++; }
  void mark_error_404() { metrics_.error_404_count++; }
  void mark_error_500() { metrics_.error_500_count++; }
  void mark_error_403() { metrics_.error_403_count++; }

private:
  Metrics&                              metrics_;
  std::chrono::steady_clock::time_point start_time_;
};

} // namespace libwavy::server
