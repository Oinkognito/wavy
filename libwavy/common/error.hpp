#pragma once

#include <backtrace.h>
#include <cxxabi.h>
#include <dlfcn.h>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

namespace libwavy
{

class StackTrace
{
public:
  explicit StackTrace(size_t skip = 0, size_t max_frames = 64)
  {
    ensure_state();
    backtrace_full(state_, static_cast<int>(skip + 1), &StackTrace::full_callback,
                   &StackTrace::error_callback, this);
  }

  [[nodiscard]] auto to_string() const -> std::string
  {
    std::ostringstream out;
    for (const auto& frame : frames_)
    {
      out << "  at " << frame << "\n";
    }
    return out.str();
  }

private:
  std::vector<std::string>       frames_;
  static inline backtrace_state* state_ = nullptr;
  static inline std::mutex       state_mutex_;

  static void ensure_state()
  {
    std::lock_guard<std::mutex> lock(state_mutex_);
    if (!state_)
    {
      state_ = backtrace_create_state(nullptr, /* threaded = */ 1, nullptr, nullptr);
    }
  }

  static auto full_callback(void* data, uintptr_t pc, const char* filename, int lineno,
                            const char* function) -> int
  {
    auto* self = static_cast<StackTrace*>(data);

    std::ostringstream frame;

    // Function name (demangled if possible)
    if (function)
      frame << demangle(function);
    else
      frame << "<unknown>";

    // File/line info
    if (filename)
      frame << " (" << filename << ":" << lineno << ")";
    else
      frame << " (no source info)";

    // Module info (dladdr)
    Dl_info info;
    if (dladdr(reinterpret_cast<void*>(pc), &info) && info.dli_fname)
    {
      frame << " [" << info.dli_fname;
      if (info.dli_sname)
        frame << ":" << demangle(info.dli_sname);
      frame << "]";
    }

    // Always include raw PC
    frame << " [pc=" << reinterpret_cast<void*>(pc) << "]";

    self->frames_.push_back(frame.str());
    return 0;
  }

  static void error_callback(void* /*data*/, const char* msg, int errnum)
  {
    std::ostringstream out;
    out << "<error: " << msg;
    if (errnum != 0)
      out << " (errno=" << errnum << ")";
    out << ">";
  }

  static auto demangle(const char* name) -> std::string
  {
    int         status    = 0;
    size_t      len       = 0;
    char*       demangled = abi::__cxa_demangle(name, nullptr, &len, &status);
    std::string result    = (status == 0 && demangled) ? demangled : name;
    free(demangled);
    return result;
  }
};

} // namespace libwavy
