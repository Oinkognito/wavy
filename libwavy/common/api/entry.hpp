#pragma once

#if defined(_WIN32) || defined(_WIN64)
#define WAVY_PLATFORM_WINDOWS 1
#if defined(_WIN64)
#define WAVY_PLATFORM_64BIT 1
#else
#define WAVY_PLATFORM_32BIT 1
#endif
#elif defined(__linux__)
#define WAVY_PLATFORM_LINUX 1
#if defined(__x86_64__) || defined(__aarch64__)
#define WAVY_PLATFORM_64BIT 1
#else
#define WAVY_PLATFORM_32BIT 1
#endif
#elif defined(__APPLE__)
#define WAVY_PLATFORM_APPLE 1
#include <TargetConditionals.h>
#if TARGET_OS_MAC
#define WAVY_PLATFORM_MACOS 1
#endif
#if defined(__x86_64__) || defined(__aarch64__)
#define WAVY_PLATFORM_64BIT 1
#else
#define WAVY_PLATFORM_32BIT 1
#endif
#endif

// Visibility
#if defined(_WIN32)
#ifdef WAVY_EXPORTS
#define WAVY_API __declspec(dllexport)
#else
#define WAVY_API __declspec(dllimport)
#endif
#else
#define WAVY_API __attribute__((visibility("default")))
#endif

#define WAVY_NODISCARD [[nodiscard]]

#if defined(__has_cpp_attribute) && __has_cpp_attribute(deprecated)
#define WAVY_DEPRECATED(msg) [[deprecated(msg)]]
#elif defined(WAVY_COMPILER_GCC) || defined(WAVY_COMPILER_CLANG)
#define WAVY_DEPRECATED(msg) __attribute__((deprecated(msg)))
#elif defined(WAVY_COMPILER_MSVC)
#define WAVY_DEPRECATED(msg) __declspec(deprecated(msg))
#else
#define WAVY_DEPRECATED(msg)
#endif

#define WAVY_FORCE_INLINE inline __attribute__((always_inline))
#define WAVY_NO_INLINE    __attribute__((noinline))
#define WAVY_UNUSED(x)    (void)(x)
#define WAVY_LIKELY(x)    __builtin_expect(!!(x), 1)
#define WAVY_UNLIKELY(x)  __builtin_expect(!!(x), 0)
