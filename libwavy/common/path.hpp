#pragma once

#include <filesystem>
#include <ostream>
#include <string>
#include <string_view>
#include <utility>

namespace libwavy
{

/// Base CRTP wrapper around std::filesystem::path
template <typename Derived> class PathBase
{
protected:
  std::filesystem::path path_;

public:
  PathBase() = default;
  PathBase(const char* s) : path_(s) {}
  PathBase(const std::string& s) : path_(s) {}
  PathBase(std::string_view sv) : path_(sv) {}
  PathBase(std::filesystem::path p) : path_(std::move(p)) {}

  // String conversions
  [[nodiscard]] auto str() const -> std::string { return path_.string(); }
  operator std::string() const { return path_.string(); }
  operator std::filesystem::path() const { return path_; }

  // Access as C-string
  [[nodiscard]] auto c_str() const noexcept -> const char* { return path_.c_str(); }

  // Access underlying path
  [[nodiscard]] auto native() const -> const std::filesystem::path& { return path_; }

  // Append / operator
  template <typename T> auto operator/(const T& rhs) const -> Derived
  {
    return Derived(path_ / std::filesystem::path(rhs));
  }

  template <typename T> auto operator+(const T& rhs) const -> Derived
  {
    return Derived(path_.string() + std::string(rhs));
  }

  // Comparisons
  auto operator==(const PathBase& other) const -> bool { return path_ == other.path_; }
  auto operator!=(const PathBase& other) const -> bool { return path_ != other.path_; }
  auto operator<(const PathBase& other) const -> bool { return path_ < other.path_; }

  // Iterators
  [[nodiscard]] auto begin() const { return path_.begin(); }
  [[nodiscard]] auto end() const { return path_.end(); }

  // Filename / extension helpers
  [[nodiscard]] auto filename() const -> std::string { return path_.filename().string(); }
  [[nodiscard]] auto stem() const -> std::string { return path_.stem().string(); }
  [[nodiscard]] auto extension() const -> std::string { return path_.extension().string(); }

  // Existence check
  [[nodiscard]] auto exists() const -> bool { return std::filesystem::exists(path_); }

  // Ends_with helpers
  [[nodiscard]] auto ends_with(std::string_view suffix) const -> bool
  {
    auto s = str();
    return s.size() >= suffix.size() &&
           s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
  }

  [[nodiscard]] auto ends_with(const std::string& suffix) const -> bool
  {
    return ends_with(std::string_view(suffix));
  }

  [[nodiscard]] auto ends_with(const char* suffix) const -> bool
  {
    return ends_with(std::string_view(suffix));
  }

  // String-like helpers
  [[nodiscard]] auto find_last_of(std::string_view chars) const -> std::size_t
  {
    return str().find_last_of(chars);
  }

  [[nodiscard]] auto find_last_of(const std::string& chars) const -> std::size_t
  {
    return str().find_last_of(chars);
  }

  [[nodiscard]] auto find_last_of(const char* chars) const -> std::size_t
  {
    return str().find_last_of(chars);
  }

  [[nodiscard]] auto substr(std::size_t pos, std::size_t n = std::string::npos) const -> std::string
  {
    return str().substr(pos, n);
  }

  // Friend for streaming
  friend auto operator<<(std::ostream& os, const PathBase& p) -> std::ostream&
  {
    return os << p.path_;
  }
};

class AbsPath : public PathBase<AbsPath>
{
public:
  using PathBase<AbsPath>::PathBase;
};

using AbsPathStr = std::string; // copy constrible version of AbsPath

} // namespace libwavy
