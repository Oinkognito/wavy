#pragma once

#include <cxxabi.h>
#include <execinfo.h>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace libwavy
{

class StackTrace
{
public:
  explicit StackTrace(size_t max_frames = 64)
  {
    void* buffer[64];
    int   nptrs = ::backtrace(buffer, static_cast<int>(max_frames));
    frames_.assign(buffer, buffer + nptrs);
  }

  [[nodiscard]] auto to_string() const -> std::string
  {
    char** symbols = ::backtrace_symbols(frames_.data(), static_cast<int>(frames_.size()));
    if (!symbols)
      return "Failed to get stack trace symbols\n";

    std::ostringstream out;
    for (size_t i = 0; i < frames_.size(); ++i)
    {
      out << "#" << i << " " << demangle(symbols[i]) << "\n";
    }

    free(symbols);
    return out.str();
  }

private:
  std::vector<void*> frames_;

  static auto demangle(const char* symbol) -> std::string
  {
    // Try to extract mangled name between '(' and '+'
    const char* begin = nullptr;
    const char* end   = nullptr;
    for (const char* p = symbol; *p; ++p)
    {
      if (*p == '(')
        begin = p;
      else if (*p == '+')
        end = p;
    }

    if (begin && end && begin < end)
    {
      ++begin;
      int                                    status = 0;
      size_t                                 len    = 0;
      std::unique_ptr<char, void (*)(void*)> demangled(
        abi::__cxa_demangle(begin, nullptr, &len, &status), std::free);

      if (status == 0 && demangled)
      {
        return std::string(symbol, begin - 1) + demangled.get() + end;
      }
    }
    return symbol; // Fallback to raw
  }
};

} // namespace libwavy
