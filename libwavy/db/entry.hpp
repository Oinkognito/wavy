#pragma once

#include <atomic>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <libwavy/common/types.hpp>
#include <libwavy/db/error.hpp>
#include <libwavy/db/structs.hpp>
#include <mutex>
#include <optional>
#include <shared_mutex>

namespace libwavy::db
{

using KV_Callback     = std::function<void(const Key&, const Value&)>;
using KV_ViewCallback = std::function<void(const KeyView&, const ValueView&)>;
using K_Callback      = std::function<void(const Key&)>;
using Batch_CallBack  = std::function<void(KV_Callback, K_Callback)>;

template <typename Extra>
  requires std::is_trivially_copyable_v<Extra>
class LMDBKV
{
public:
  using MetaType = Meta<Extra>;
  using Clock    = std::chrono::system_clock;
  mutable std::unordered_map<LMDBKV*, std::shared_ptr<MDB_txn>> read_txn_pool_;

  LMDBKV(const std::filesystem::path& dir, std::size_t initial_map_size = 64ull * 1024 * 1024)
      : map_size_(initial_map_size), update_counter_(0)
  {
    namespace fs = std::filesystem;
    if (!fs::exists(dir))
      fs::create_directories(dir);

    int rc = mdb_env_create(&env_);
    if (rc != MDB_SUCCESS)
      throw LMDBError("mdb_env_create failed", rc);

    rc = mdb_env_set_maxdbs(env_, 2); // data + metadata
    if (rc != MDB_SUCCESS)
      throw LMDBError("mdb_env_set_maxdbs failed", rc);

    rc = mdb_env_set_mapsize(env_, static_cast<size_t>(map_size_));
    if (rc != MDB_SUCCESS)
      throw LMDBError("mdb_env_set_mapsize failed", rc);

    rc = mdb_env_open(env_, dir.string().c_str(), 0, 0664);
    if (rc != MDB_SUCCESS)
      throw LMDBError("mdb_env_open failed", rc);

    // open DBIs
    MDB_txn* txn = nullptr;
    rc           = mdb_txn_begin(env_, nullptr, 0, &txn);
    if (rc != MDB_SUCCESS)
      throw LMDBError("mdb_txn_begin open dbis failed", rc);

    rc = mdb_dbi_open(txn, "data", MDB_CREATE, &dbi_data_);
    if (rc != MDB_SUCCESS)
    {
      mdb_txn_abort(txn);
      throw LMDBError("mdb_dbi_open data failed", rc);
    }

    rc = mdb_dbi_open(txn, "meta", MDB_CREATE, &dbi_meta_);
    if (rc != MDB_SUCCESS)
    {
      mdb_txn_abort(txn);
      throw LMDBError("mdb_dbi_open meta failed", rc);
    }

    rc = mdb_txn_commit(txn);
    if (rc != MDB_SUCCESS)
      throw LMDBError("mdb_txn_commit open dbis failed", rc);
  }

  ~LMDBKV()
  {
    if (env_)
    {
      mdb_dbi_close(env_, dbi_data_);
      mdb_dbi_close(env_, dbi_meta_);
      mdb_env_close(env_);
    }
  }

  // Zero-copy get_view. ValueView keeps txn alive until destroyed.
  auto get_view(const Key& key) const -> std::optional<ValueView>
  {
    MDB_txn* txn = nullptr;
    int      rc  = mdb_txn_begin(env_, nullptr, MDB_RDONLY, &txn);
    if (rc != MDB_SUCCESS)
      throw LMDBError("mdb_txn_begin get_view failed", rc);

    MDB_val mk = make_mdb_val(key);
    MDB_val mv;
    rc = mdb_get(txn, dbi_data_, &mk, &mv);
    if (rc == MDB_NOTFOUND)
    {
      mdb_txn_abort(txn);
      return std::nullopt;
    }
    if (rc != MDB_SUCCESS)
    {
      mdb_txn_abort(txn);
      throw LMDBError("mdb_get failed in get_view", rc);
    }

    // Wrap txn into shared_ptr so txn stays valid until ValueView goes out of scope
    auto txn_owner = std::shared_ptr<MDB_txn>(txn, [](MDB_txn* t) { mdb_txn_abort(t); });

    return ValueView{
      .data = static_cast<const char*>(mv.mv_data), .size = mv.mv_size, .txn_owner = txn_owner};
  }

  // Delete key + metadata
  void erase(const Key& key)
  {
    std::unique_lock lock(write_mutex_);
    if (!write_with_resize(
          [&](MDB_txn* txn)
          {
            MDB_val mk = make_mdb_val(key);
            int     rc = mdb_del(txn, dbi_data_, &mk, nullptr);
            if (rc != MDB_SUCCESS && rc != MDB_NOTFOUND)
              return rc;
            rc = mdb_del(txn, dbi_meta_, &mk, nullptr);
            if (rc != MDB_SUCCESS && rc != MDB_NOTFOUND)
              return rc;
            return MDB_SUCCESS;
          }))
    {
      throw LMDBError("Something went wrong while erasing!");
    }
  }

  // Range query: iterate keys in [start, end) (lexicographic)
  void range_query(const Key& start, const Key& end, KV_Callback fn) const
  {
    MDB_txn* txn = nullptr;
    int      rc  = mdb_txn_begin(env_, nullptr, MDB_RDONLY, &txn);
    if (rc != MDB_SUCCESS)
      throw LMDBError("mdb_txn_begin range_query failed", rc);

    MDB_cursor* cursor = nullptr;
    rc                 = mdb_cursor_open(txn, dbi_data_, &cursor);
    if (rc != MDB_SUCCESS)
    {
      mdb_txn_abort(txn);
      throw LMDBError("mdb_cursor_open failed", rc);
    }

    MDB_val k = make_mdb_val(start);
    MDB_val v;
    rc = mdb_cursor_get(cursor, &k, &v, MDB_SET_RANGE);
    while (rc == MDB_SUCCESS)
    {
      std::string keyStr(static_cast<char*>(k.mv_data), k.mv_size);
      if (!end.empty() && keyStr >= end)
        break;
      Value val(static_cast<char*>(v.mv_data), static_cast<char*>(v.mv_data) + v.mv_size);
      fn(keyStr, val);
      rc = mdb_cursor_get(cursor, &k, &v, MDB_NEXT);
    }

    if (rc != MDB_NOTFOUND && rc != MDB_SUCCESS)
    {
      mdb_cursor_close(cursor);
      mdb_txn_abort(txn);
      throw LMDBError("cursor error in range_query", rc);
    }

    mdb_cursor_close(cursor);
    mdb_txn_abort(txn);
  }

  // Zero-copy range query: yields KeyView + ValueView
  void range_query_view(const Key& start, const Key& end, KV_ViewCallback fn) const
  {
    MDB_txn* txn = nullptr;
    int      rc  = mdb_txn_begin(env_, nullptr, MDB_RDONLY, &txn);
    if (rc != MDB_SUCCESS)
      throw LMDBError("mdb_txn_begin range_query_view failed", rc);

    MDB_cursor* cursor = nullptr;
    rc                 = mdb_cursor_open(txn, dbi_data_, &cursor);
    if (rc != MDB_SUCCESS)
    {
      mdb_txn_abort(txn);
      throw LMDBError("mdb_cursor_open failed", rc);
    }

    MDB_val k = make_mdb_val(start);
    MDB_val v;
    rc = mdb_cursor_get(cursor, &k, &v, MDB_SET_RANGE);

    // Shared owner for txn (keeps txn alive for all ValueView lifetimes)
    auto txn_owner = std::shared_ptr<MDB_txn>(txn, [](MDB_txn* t) { mdb_txn_abort(t); });

    while (rc == MDB_SUCCESS)
    {
      KeyView keyView{.data = static_cast<const char*>(k.mv_data), .size = k.mv_size};

      // End condition
      if (!end.empty())
      {
        Key kstr(static_cast<const char*>(k.mv_data), k.mv_size);
        if (kstr >= end)
          break;
      }

      ValueView valView{
        .data = static_cast<const char*>(v.mv_data), .size = v.mv_size, .txn_owner = txn_owner};

      fn(keyView, valView);

      rc = mdb_cursor_get(cursor, &k, &v, MDB_NEXT);
    }

    if (rc != MDB_NOTFOUND && rc != MDB_SUCCESS)
    {
      mdb_cursor_close(cursor);
      throw LMDBError("cursor error in range_query_view", rc);
    }

    mdb_cursor_close(cursor);
    // txn is not aborted here; it will be released when txn_owner
    // (captured in all ValueView returned to caller) goes out of scope.
  }

  // Put value (atomically update metadata: version++, timestamp)
  void put(const Key& key, const Value& value)
  {
    // Writers are serialized
    std::unique_lock lock(write_mutex_);
    if (!write_with_resize(
          [&](MDB_txn* txn)
          {
            MDB_val mk = make_mdb_val(key);
            MDB_val mv = make_mdb_val(value);
            int     rc = mdb_put(txn, dbi_data_, &mk, &mv, 0);
            if (rc != MDB_SUCCESS)
              return rc;

            // update metadata: read current meta, increment version
            MetaType meta = read_meta_txn(txn, key).value_or(MetaType{.version = 0, .ts_unix = 0});
            meta.version++;
            meta.ts_unix  = static_cast<std::uint64_t>(Clock::to_time_t(Clock::now()));
            MDB_val mmeta = make_mdb_val(serialize_meta(meta));
            rc            = mdb_put(txn, dbi_meta_, &mk, &mmeta, 0);
            return rc;
          }))
    {
      throw LMDBError("Something went wrong while putting Key!");
    }
  }

  // Put by reading a file from disk
  void put(const std::filesystem::path& filepath)
  {
    if (!std::filesystem::exists(filepath))
      throw LMDBError("file not found");
    std::ifstream ifs(filepath, std::ios::binary);
    if (!ifs)
      throw LMDBError("failed opening file");
    Value buf((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    put(filepath.filename().string(), buf);
  }

  // Get copy (throws if not found)
  auto get(const std::string& key) -> Value
  {
    MDB_txn* txn;
    if (mdb_txn_begin(env_, nullptr, MDB_RDONLY, &txn) != MDB_SUCCESS)
      throw std::runtime_error("Failed to start txn");

    MDB_val k{key.size(), const_cast<char*>(key.data())};
    MDB_val v;
    if (mdb_get(txn, dbi_data_, &k, &v) != MDB_SUCCESS)
    {
      mdb_txn_commit(txn); // commit read txn
      throw std::runtime_error("Key not found");
    }

    Value data((char*)v.mv_data, (char*)v.mv_data + v.mv_size);
    mdb_txn_commit(txn); // commit instead of abort
    return data;
  }

  auto exists(const Key& key) const -> bool
  {
    MDB_txn* txn = nullptr;
    int      rc  = mdb_txn_begin(env_, nullptr, MDB_RDONLY, &txn);
    if (rc != MDB_SUCCESS)
      throw LMDBError("mdb_txn_begin exists failed", rc);
    MDB_val mk = make_mdb_val(key);
    MDB_val mv;
    bool    found = (mdb_get(txn, dbi_data_, &mk, &mv) == MDB_SUCCESS);
    mdb_txn_abort(txn);
    return found;
  }

  // Get metadata (version + timestamp) if any
  auto meta(const Key& key) const -> std::optional<MetaType>
  {
    MDB_txn* txn = nullptr;
    int      rc  = mdb_txn_begin(env_, nullptr, MDB_RDONLY, &txn);
    if (rc != MDB_SUCCESS)
      throw LMDBError("mdb_txn_begin meta failed", rc);
    auto res = read_meta_txn(txn, key);
    mdb_txn_abort(txn);
    return res;
  }

  auto update_meta(const Key& key, const Extra& extra) -> bool
  {
    std::unique_lock lock(write_mutex_);
    return write_with_resize(
      [&](MDB_txn* txn)
      {
        MDB_val mk = make_mdb_val(key);

        // Load existing meta
        auto metaOpt = read_meta_txn(txn, key);
        if (!metaOpt)
          return MDB_NOTFOUND;

        MetaType meta = *metaOpt;
        meta.extra    = extra;

        // Write back
        MDB_val mmeta = make_mdb_val(serialize_meta(meta));
        int     rc    = mdb_put(txn, dbi_meta_, &mk, &mmeta, 0);
        return rc;
      });
  }

  void print_meta(const Key& key, std::function<void(std::ostream&, const Extra&)> printer) const
  {
    auto m = meta(key);
    if (!m)
    {
      std::cout << "No metadata for key=" << key << "\n";
      return;
    }

    std::cout << "{version=" << m->version << ", ts=" << m->ts_unix << ", extra=";
    if (printer)
    {
      printer(std::cout, m->extra);
    }
    else
    {
      std::cout << "<no printer>";
    }
    std::cout << "}" << std::endl;
  }

  // Iteration: call fn(key, value) for every key in DB. If prefix is not empty, iterate keys starting with prefix.
  // WARNING: iteration uses a read txn and the memory returned by value/view is valid until fn returns.
  void for_each(const std::string& prefix, KV_Callback fn) const
  {
    MDB_txn* txn = nullptr;
    int      rc  = mdb_txn_begin(env_, nullptr, MDB_RDONLY, &txn);
    if (rc != MDB_SUCCESS)
      throw LMDBError("mdb_txn_begin iterate failed", rc);

    MDB_cursor* cursor = nullptr;
    rc                 = mdb_cursor_open(txn, dbi_data_, &cursor);
    if (rc != MDB_SUCCESS)
    {
      mdb_txn_abort(txn);
      throw LMDBError("mdb_cursor_open failed", rc);
    }

    MDB_val       k, v;
    MDB_cursor_op op = MDB_FIRST;
    if (!prefix.empty())
    {
      // position to first key >= prefix
      k  = make_mdb_val(prefix);
      op = MDB_SET_RANGE;
    }

    while ((rc = mdb_cursor_get(cursor, &k, &v, op)) == MDB_SUCCESS)
    {
      op = MDB_NEXT;
      std::string keyStr(static_cast<char*>(k.mv_data), k.mv_size);
      if (!prefix.empty())
      {
        if (!keyStr.starts_with(prefix))
          break; // prefix mismatch
      }
      Value val(static_cast<char*>(v.mv_data), static_cast<char*>(v.mv_data) + v.mv_size);
      fn(keyStr, val);
    }

    mdb_cursor_close(cursor);
    mdb_txn_abort(txn);
    if (rc != MDB_NOTFOUND && rc != MDB_SUCCESS)
    {
      throw LMDBError("cursor iteration error", rc);
    }
  }

  void for_(KV_Callback fn) const { for_each("", std::move(fn)); }

  // Batch: run multiple operations inside a single write transaction. The lambda receives a simple API
  // object allowing put/delete operations that operate on the same txn. If lambda throws, transaction is aborted.
  void batch(Batch_CallBack ops)
  {
    std::unique_lock lock(write_mutex_);
    write_with_resize(
      [&](MDB_txn* txn)
      {
        // Provide two simple callables to ops to act directly on the txn
        auto put_in_txn = [&](const Key& k, const Value& val)
        {
          MDB_val mk = make_mdb_val(k);
          MDB_val mv = make_mdb_val(val);
          int     rc = mdb_put(txn, dbi_data_, &mk, &mv, 0);
          if (rc != MDB_SUCCESS)
            throw LMDBError("batch put failed", rc);
          // update meta for each put
          MetaType meta = read_meta_txn(txn, k).value_or(MetaType{.version = 0, .ts_unix = 0});
          meta.version++;
          meta.ts_unix  = static_cast<std::uint64_t>(Clock::to_time_t(Clock::now()));
          MDB_val mmeta = make_mdb_val(serialize_meta(meta));
          rc            = mdb_put(txn, dbi_meta_, &mk, &mmeta, 0);
          if (rc != MDB_SUCCESS)
            throw LMDBError("batch meta put failed", rc);
        };
        auto del_in_txn = [&](const Key& k)
        {
          MDB_val mk = make_mdb_val(k);
          int     rc = mdb_del(txn, dbi_data_, &mk, nullptr);
          if (rc != MDB_SUCCESS && rc != MDB_NOTFOUND)
            throw LMDBError("batch del failed", rc);
          // remove meta
          rc = mdb_del(txn, dbi_meta_, &mk, nullptr);
          if (rc != MDB_SUCCESS && rc != MDB_NOTFOUND)
            throw LMDBError("batch meta del failed", rc);
        };

        // ops may throw; propagate rc back to caller by catching exceptions in caller, so here just run
        try
        {
          ops(put_in_txn, del_in_txn);
        }
        catch (...)
        {
          return MDB_PANIC; // will be treated as failure by caller
        }
        return MDB_SUCCESS;
      });
  }

  auto was_updated() -> bool
  {
    ui64 current = update_counter_.load(std::memory_order_relaxed);
    if (current != last_seen_counter_)
    {
      last_seen_counter_ = current;
      return true; // updated since last check
    }
    return false; // no updates
  }

  auto get_update_counter() const -> ui64
  {
    return update_counter_.load(std::memory_order_relaxed);
  }

  // ensure mapsize at least 'new_size' (bytes). Safe to call concurrently; only writer should call to grow.
  void ensure_map_size(std::size_t new_size)
  {
    std::unique_lock lock(write_mutex_);
    if (new_size <= map_size_)
      return;
    int rc = mdb_env_set_mapsize(env_, new_size);
    if (rc != MDB_SUCCESS)
      throw LMDBError("mdb_env_set_mapsize failed", rc);
    map_size_ = new_size;
  }

private:
  MDB_env* env_{};
  MDB_dbi  dbi_data_{};
  MDB_dbi  dbi_meta_{};
  mutable std::shared_mutex
                    rw_mutex_;    // for future fine-grained read/write control (not heavily used)
  std::mutex        write_mutex_; // serialize writers
  std::size_t       map_size_;
  std::atomic<ui64> update_counter_;
  ui64              last_seen_counter_{0};

  static auto make_mdb_val(const Key& k) -> MDB_val
  {
    MDB_val mv;
    mv.mv_size = k.size();
    mv.mv_data = const_cast<char*>(k.data());
    return mv;
  }

  template <typename T> static auto make_mdb_val(const T& val) -> MDB_val
  {
    return MDB_val{val.size(), const_cast<char*>(reinterpret_cast<const char*>(val.data()))};
  }

  // serialize Meta to small binary blob: version(uint64_t) + ts(uint64_t) + extra
  static auto serialize_meta(const MetaType& m) -> std::string
  {
    std::string out;
    out.resize(sizeof(uint64_t) * 2 + sizeof(Extra));

    std::memcpy(out.data(), &m.version, sizeof(m.version));
    std::memcpy(out.data() + sizeof(m.version), &m.ts_unix, sizeof(m.ts_unix));
    std::memcpy(out.data() + sizeof(uint64_t) * 2, &m.extra, sizeof(Extra));

    return out;
  }

  static auto deserialize_meta(const MDB_val& mv) -> MetaType
  {
    MetaType m{};
    if (mv.mv_size >= static_cast<ssize_t>(sizeof(uint64_t) * 2 + sizeof(Extra)))
    {
      std::memcpy(&m.version, mv.mv_data, sizeof(m.version));
      std::memcpy(&m.ts_unix, static_cast<char*>(mv.mv_data) + sizeof(m.version),
                  sizeof(m.ts_unix));
      std::memcpy(&m.extra, static_cast<char*>(mv.mv_data) + sizeof(uint64_t) * 2, sizeof(Extra));
    }
    return m;
  }

  auto read_meta_txn(MDB_txn* txn, const Key& k) const -> std::optional<MetaType>
  {
    MDB_val mk = make_mdb_val(k);
    MDB_val mv;
    int     rc = mdb_get(txn, dbi_meta_, &mk, &mv);
    if (rc != MDB_SUCCESS)
      return std::nullopt;
    return deserialize_meta(mv);
  }

  // Writes are routed through this helper that attempts the write and on MDB_MAP_FULL will grow map size and retry.
  template <typename F> auto write_with_resize(F fn) -> bool
  {
    // try up to a few times with exponential growth
    for (int attempt = 0; attempt < 6; ++attempt)
    {
      MDB_txn* txn = nullptr;
      int      rc  = mdb_txn_begin(env_, nullptr, 0, &txn);
      if (rc != MDB_SUCCESS)
      {
        throw LMDBError("mdb_txn_begin write failed", rc);
      }

      int inner_rc = fn(txn);
      if (inner_rc == MDB_PANIC)
      {
        mdb_txn_abort(txn);
        throw std::runtime_error("batch operation signaled failure");
      }

      if (inner_rc != MDB_SUCCESS)
      {
        mdb_txn_abort(txn);

        if (inner_rc == MDB_MAP_FULL)
        {
          grow_map_size_backoff(attempt);
          continue; // retry
        }
        throw LMDBError("write op failed", inner_rc);
      }

      rc = mdb_txn_commit(txn);
      if (rc == MDB_MAP_FULL)
      {
        grow_map_size_backoff(attempt);
        continue; // retry
      }
      if (rc != MDB_SUCCESS)
      {
        throw LMDBError("mdb_txn_commit failed", rc);
      }

      // update successful write counter
      ui64 new_count     = update_counter_.fetch_add(1, std::memory_order_relaxed) + 1;
      last_seen_counter_ = new_count;

      return true; // success
    }
    // exhausted attempts
    return false;
  }

  void grow_map_size_backoff(int attempt)
  {
    // double the map size (or add 128MB on first attempt)
    std::size_t add     = (attempt == 0) ? (128ull * 1024 * 1024) : map_size_;
    std::size_t newsize = map_size_ + add;
    if (newsize < map_size_)
      newsize = map_size_ + (128ull * 1024 * 1024); // overflow safety
    int rc = mdb_env_set_mapsize(env_, newsize);
    if (rc != MDB_SUCCESS)
      throw LMDBError("mdb_env_set_mapsize grow failed", rc);
    map_size_ = newsize;
  }
};

} // namespace libwavy::db
