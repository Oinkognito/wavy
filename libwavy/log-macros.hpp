#pragma once

#define WAVY__INTERNAL_LOGGING_IMPL
#include <libwavy/logger.hpp>
#undef WAVY__INTERNAL_LOGGING_IMPL

#include <format>

#define INIT_WAVY_LOGGER_MACROS() \
  namespace lwlog = libwavy::log; \
  using _         = lwlog::NONE;

#define INIT_WAVY_LOGGER()                                                                      \
  libwavy::log::init_logging();                                                                 \
  LOG_INFO << "Wavy logger initialized! Check WAVY_LOG_LEVEL (environment variable) for which " \
              "log level this session is on!!";

#define INIT_WAVY_LOGGER_ALL() \
  INIT_WAVY_LOGGER_MACROS();   \
  INIT_WAVY_LOGGER();

/* ------------ LOGGING MACROS --------------- */

enum class LogMode
{
  Sync,
  Async
};

template <typename T, typename = void> struct is_formattable : std::false_type
{
};

template <typename T>
struct is_formattable<T, std::void_t<decltype(std::formatter<std::remove_cvref_t<T>, char>{})>>
    : std::true_type
{
};

template <typename... Args> constexpr bool all_formattable_v = (is_formattable<Args>::value && ...);

#define LOG_ARGS_TYPE_CHECK()                                                                    \
  static_assert(                                                                                 \
    all_formattable_v<Args...>,                                                                  \
    "One or more arguments passed to LOG MACROS are not formattable with std::format. Consider " \
    "converting types like std::filesystem::path to string using .string().");

namespace libwavy::log
{

// ---- INFO ----
template <typename Tag, typename... Args>
inline void INFO(LogMode mode, std::string_view fmt, Args&&... args)
{
  LOG_ARGS_TYPE_CHECK();

  auto formatted = std::vformat(fmt, std::make_format_args(args...));
  if (mode == LogMode::Async)
    LOG_INFO_ASYNC << log_prefix<Tag>() << formatted;
  else
    LOG_INFO << log_prefix<Tag>() << formatted;
}

// ---- ERROR ----
template <typename Tag, typename... Args>
inline void ERROR(LogMode mode, std::string_view fmt, Args&&... args)
{
  LOG_ARGS_TYPE_CHECK();

  auto formatted = std::vformat(fmt, std::make_format_args(args...));
  if (mode == LogMode::Async)
    LOG_ERROR_ASYNC << log_prefix<Tag>() << formatted;
  else
    LOG_ERROR << log_prefix<Tag>() << formatted;
}

// ---- DEBUG ----
template <typename Tag, typename... Args>
inline void DBG(LogMode mode, std::string_view fmt, Args&&... args)
{
  LOG_ARGS_TYPE_CHECK();

  auto formatted = std::vformat(fmt, std::make_format_args(args...));
  if (mode == LogMode::Async)
    LOG_DEBUG_ASYNC << log_prefix<Tag>() << formatted;
  else
    LOG_DEBUG << log_prefix<Tag>() << formatted;
}

// ---- TRACE ----
template <typename Tag, typename... Args>
inline void TRACE(LogMode mode, std::string_view fmt, Args&&... args)
{
  LOG_ARGS_TYPE_CHECK();

  auto formatted = std::vformat(fmt, std::make_format_args(args...));
  if (mode == LogMode::Async)
    LOG_TRACE_ASYNC << log_prefix<Tag>() << formatted;
  else
    LOG_TRACE << log_prefix<Tag>() << formatted;
}

// ---- WARN ----
template <typename Tag, typename... Args>
inline void WARN(LogMode mode, std::string_view fmt, Args&&... args)
{
  LOG_ARGS_TYPE_CHECK();

  auto formatted = std::vformat(fmt, std::make_format_args(args...));
  if (mode == LogMode::Async)
    LOG_WARNING_ASYNC << log_prefix<Tag>() << formatted;
  else
    LOG_WARNING << log_prefix<Tag>() << formatted;
}

// ---- Default Sync Overloads ----

template <typename Tag, typename... Args> inline void INFO(std::string_view fmt, Args&&... args)
{
  INFO<Tag>(LogMode::Sync, fmt, std::forward<Args>(args)...);
}

template <typename Tag, typename... Args> inline void ERROR(std::string_view fmt, Args&&... args)
{
  ERROR<Tag>(LogMode::Sync, fmt, std::forward<Args>(args)...);
}

template <typename Tag, typename... Args> inline void DBG(std::string_view fmt, Args&&... args)
{
  DBG<Tag>(LogMode::Sync, fmt, std::forward<Args>(args)...);
}

template <typename Tag, typename... Args> inline void TRACE(std::string_view fmt, Args&&... args)
{
  TRACE<Tag>(LogMode::Sync, fmt, std::forward<Args>(args)...);
}

template <typename Tag, typename... Args> inline void WARN(std::string_view fmt, Args&&... args)
{
  WARN<Tag>(LogMode::Sync, fmt, std::forward<Args>(args)...);
}

} // namespace libwavy::log
