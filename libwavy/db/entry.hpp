#pragma once

#include <functional>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace libwavy
{

template <typename Owner, typename AudioID, typename OwnerHash = std::hash<Owner>,
          typename OwnerEq = std::equal_to<Owner>, typename AudioHash = std::hash<AudioID>,
          typename AudioEq = std::equal_to<AudioID>>
class MiniDB
{
public:
  using owner_type   = Owner;
  using audio_type   = AudioID;
  using audio_set    = std::unordered_set<AudioID, AudioHash, AudioEq>;
  using storage_type = std::unordered_map<Owner, audio_set, OwnerHash, OwnerEq>;

private:
  storage_type data_;
  bool         db_initialized_ = false;
  bool         modified_       = false; // insert/delete marks this true

public:
  MiniDB() = default;

  // Single-use initialization function
  void update_db(std::function<void(MiniDB&)> init_fn)
  {
    if (modified_)
      throw std::runtime_error("DB already modified. Cannot call update_db after insert/delete.");

    if (db_initialized_)
      throw std::runtime_error("update_db can only be called once.");

    if (!init_fn)
      throw std::invalid_argument("Initialization function cannot be empty.");

    init_fn(*this);
    db_initialized_ = true;
  }

  // Insert relation (Owner -> AudioID)
  auto insert(const Owner& owner, const AudioID& audio) -> bool
  {
    modified_ = true;
    return data_[owner].insert(audio).second; // true if inserted new audio_id
  }

  // Check if owner exists
  auto has_owner(const Owner& owner) const -> bool { return data_.find(owner) != data_.end(); }

  // Check if (owner, audio) relation exists
  auto has(const Owner& owner, const AudioID& audio) const -> bool
  {
    auto it = data_.find(owner);
    if (it == data_.end())
      return false;
    return it->second.find(audio) != it->second.end();
  }

  // Get all audio IDs for an owner (const ref)
  auto audio_ids(const Owner& owner) const -> const audio_set&
  {
    static const audio_set empty{};
    auto                   it = data_.find(owner);
    return it == data_.end() ? empty : it->second;
  }

  // Iterate over all owners
  auto owners() const
  {
    std::vector<Owner> result;
    result.reserve(data_.size());
    for (auto& [owner, _] : data_)
      result.push_back(owner);
    return result;
  }

  // Apply fn(owner, audio_id) for all relations
  template <typename Fn> void for_each(Fn&& fn) const
  {
    for (auto& [owner, audios] : data_)
    {
      for (auto& audio : audios)
      {
        fn(owner, audio);
      }
    }
  }

  // Apply fn(owner, audio_set) for all owners
  template <typename Fn> void for_each_owner(Fn&& fn) const
  {
    for (auto& [owner, audios] : data_)
    {
      fn(owner, audios);
    }
  }

  // Number of owners
  [[nodiscard]] auto owner_count() const -> size_t { return data_.size(); }

  // Number of total relations
  [[nodiscard]] auto relation_count() const -> size_t
  {
    size_t total = 0;
    for (auto& [_, audios] : data_)
      total += audios.size();
    return total;
  }

  void clear()
  {
    data_.clear();
    modified_ = true;
  }
};

} // namespace libwavy
