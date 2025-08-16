#pragma once

#include "libwavy/common/types.hpp"
#include <functional>
#include <iomanip>
#include <iostream>
#include <lmdb.h>
#include <memory>
#include <vector>

namespace libwavy::db
{

using Key      = const std::string;
using MutValue = std::vector<char>;
using Value    = const std::vector<char>;

struct as
{
  static auto key(const libwavy::db::Value& v) -> libwavy::db::Key { return {v.begin(), v.end()}; }
  static auto value(const libwavy::db::Key& k) -> libwavy::db::Value
  {
    return {k.begin(), k.end()};
  }
};

inline auto make_kv_key(const StorageOwnerID& owner, const StorageAudioID& audio_id,
                        const std::string& fname) -> Key
{
  return owner + "/" + audio_id + "/" + fname;
}

struct ValueView
{
  // zero-copy view into LMDB memory; valid while txn is alive
  const char* data{nullptr};
  std::size_t size{0};

  // Keep txn alive by owning it (shared_ptr)
  std::shared_ptr<MDB_txn> txn_owner; // custom-managed pointer
  // Custom deleter required when freeing txn: we'll use a wrapper so abort/commit not done here.
};

struct KeyView
{
  const char* data;
  size_t      size;
};

template <typename T>
concept Streamable = requires(std::ostream& os, const T& t) {
  {
    os << t
  } -> std::same_as<std::ostream&>;
};

// DB can only have one customizable struct at a time
template <typename Extra> struct Meta
{
  uint64_t                                                       version{0};
  std::uint64_t                                                  ts_unix{0};
  Extra                                                          extra{};
  inline static std::function<void(std::ostream&, const Extra&)> extra_printer_;

  friend auto operator<<(std::ostream& os, const Meta& m) -> std::ostream&
  {
    // Convert ts_unix -> human readable
    auto    t = static_cast<time_t>(m.ts_unix);
    std::tm tm{};
#if defined(_WIN32)
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif

    os << "{version=" << m.version << ", ts=" << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");

    if constexpr (!std::is_void_v<Extra>)
    {
      if constexpr (Streamable<Extra>)
        os << ", extra=" << m.extra;
      else
        os << ", extra=<unprintable>";
    }

    os << "}";
    return os;
  }
};
} // namespace libwavy::db
