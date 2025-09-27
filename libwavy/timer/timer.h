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

/*
 * A simple scoped timer that works for logical code blocks and functions.
 */

#include <chrono>
#include <functional>
#include <iostream>
#include <string_view>

namespace libwavy::timer
{

template <typename Duration = std::chrono::microseconds> class ScopedTimer
{
public:
  using Clock     = std::chrono::high_resolution_clock;
  using TimePoint = typename Clock::time_point;
  using Callback  = std::function<void(Duration, std::string_view)>;

  explicit ScopedTimer(std::string_view label = "", std::string_view funcName = "",
                       Callback cb = defaultCallback)
      : label_(label), funcName_(funcName), callback_(std::move(cb)), start_(Clock::now())
  {
  }

  ScopedTimer(const ScopedTimer&)                    = delete;
  auto operator=(const ScopedTimer&) -> ScopedTimer& = delete;

  ScopedTimer(ScopedTimer&& other) noexcept
      : label_(std::move(other.label_)), funcName_(std::move(other.funcName_)),
        callback_(std::move(other.callback_)), start_(other.start_), stopped_(other.stopped_)
  {
    other.stopped_ = true;
  }

  auto operator=(ScopedTimer&& other) noexcept -> ScopedTimer&
  {
    if (this != &other)
    {
      stop();
      label_         = std::move(other.label_);
      funcName_      = std::move(other.funcName_);
      callback_      = std::move(other.callback_);
      start_         = other.start_;
      stopped_       = other.stopped_;
      other.stopped_ = true;
    }
    return *this;
  }

  ~ScopedTimer() { stop(); }

  void stop()
  {
    if (!stopped_)
    {
      auto end     = Clock::now();
      auto elapsed = std::chrono::duration_cast<Duration>(end - start_);
      callback_(elapsed, funcName_.empty() ? label_ : funcName_);
      stopped_ = true;
    }
  }

private:
  std::string label_;
  std::string funcName_;
  Callback    callback_;
  TimePoint   start_;
  bool        stopped_ = false;

  static void defaultCallback(Duration d, std::string_view where)
  {
    constexpr auto reset   = "\033[0m";
    constexpr auto bold    = "\033[1m";
    constexpr auto yellow  = "\033[33m";
    constexpr auto cyan    = "\033[36m";
    constexpr auto magenta = "\033[35m";

    std::cout << "\n"
              << bold << yellow
              << "====================[ SCOPED_TIMER REPORT ]====================\n"
              << magenta << "   Location: " << cyan << (where.empty() ? "<unknown>" : where) << "\n"
              << magenta << "   Elapsed : " << cyan << d.count() << " "
              << durationSuffix<Duration>() << "\n"
              << yellow
              << "===============================================================" << reset << "\n";
  }

  template <typename D> static constexpr auto durationSuffix() -> const char*
  {
    if constexpr (std::is_same_v<D, std::chrono::nanoseconds>)
    {
      return "ns";
    }
    else if constexpr (std::is_same_v<D, std::chrono::microseconds>)
    {
      return "us";
    }
    else if constexpr (std::is_same_v<D, std::chrono::milliseconds>)
    {
      return "ms";
    }
    else if constexpr (std::is_same_v<D, std::chrono::seconds>)
    {
      return "s";
    }
    else if constexpr (std::is_same_v<D, std::chrono::minutes>)
    {
      return "min";
    }
    else if constexpr (std::is_same_v<D, std::chrono::hours>)
    {
      return "h";
    }
    else
    {
      return "<unknown-unit>";
    }
  }
};

} // namespace libwavy::timer

#define CONCAT_INTERNAL(x, y) x##y
#define CONCAT(x, y)          CONCAT_INTERNAL(x, y)

#define MEASURE_BLOCK(duration_type) \
  libwavy::timer::ScopedTimer<duration_type> CONCAT(_scoped_timer_, __COUNTER__)("", __func__)

#define MEASURE_FUNC(duration_type) \
  libwavy::timer::ScopedTimer<duration_type> CONCAT(_func_timer_, __COUNTER__)(__func__, __func__)
