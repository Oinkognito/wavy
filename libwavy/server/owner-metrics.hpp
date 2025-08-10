#pragma once

#include <atomic>
#include <libwavy/common/types.hpp>

struct OwnerMetrics
{
  std::atomic<ui64> uploads{0};
  std::atomic<ui64> downloads{0};
  std::atomic<ui64> deletes{0};
  std::atomic<ui64> songs_count{0};
  std::atomic<ui64> storage_bytes{0};
};
