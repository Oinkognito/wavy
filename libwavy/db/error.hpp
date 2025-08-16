#pragma once

#include <lmdb.h>
#include <sstream>
#include <stdexcept>
#include <string>

namespace libwavy::db
{

class LMDBError : public std::runtime_error
{
public:
  explicit LMDBError(const std::string& msg, int rc = 0)
      : std::runtime_error(build_msg(msg, rc)), code_(rc)
  {
  }
  [[nodiscard]] auto code() const noexcept -> int { return code_; }

private:
  int         code_;
  static auto build_msg(const std::string& m, int rc) -> std::string
  {
    if (rc == 0)
      return m;
    std::ostringstream oss;
    oss << m << ": " << mdb_strerror(rc) << " (" << rc << ")";
    return oss.str();
  }
};

} // namespace libwavy::db
