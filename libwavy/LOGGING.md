# Logging in Wavy (`logger.hpp`)

The `logger.hpp` file provides a structured logging system for Wavy, leveraging **Boost.Log** for both console and file-based logging. It includes features such as **color-coded severity levels**, **timestamped logs**, **per-component log tags**, and **support for multi-threaded logging** with thread-aware macros.

> [!CAUTION]
>
> Never include `libwavy/logger.hpp` this will
> cause **INTENTIONAL** breaking compilation errors!
>
> Always include:
> ```cpp
> #include <libwavy/log-macros.hpp>
> ```
>

## **1. Initialization**

> [!IMPORTANT]
>
> You can change log level at runtime!!
>
> Run the following before running any Wavy binary:
>
> ```bash
> export WAVY_LOG_LEVEL=TRACE
> ```
>
> In the above example, every log priority that is **ABOVE** `TRACE`
> will be printed logged to file!
>
> The **log priority** order:
>
> 1. `ERROR`
> 2. `WARN`
> 3. `INFO`
> 4. `TRACE`
> 5. `DEBUG`
>

Before using the logger, call:

```cpp
libwavy::log::init_logging();
```

Or use the convenience macro:

```cpp
INIT_WAVY_LOGGER();
```

> [!IMPORTANT]
>
> To just use convenient namespaces like `_`, call `INIT_WAVY_LOGGER_MACROS` like so:
>
> ```cpp
> // can be at the top of the source file after includes
> INIT_WAVY_LOGGER_MACROS();
> ```

This initializes:

* Console and file sinks
* Directory creation at `$HOME/.cache/wavy/logs`
* ANSI-colored console output
* ANSI-stripped file output
* Timestamped log entries
* Have a special typedef for `NONE` log category `_`
* Add a new namespace `lwlog` as `libwavy::log`

## **2. Logging Severity Levels**

Five severity levels are supported:

* **INFO**: General status messages (`LOG_INFO`)
* **WARNING**: Potential issues (`LOG_WARNING`)
* **ERROR**: Critical errors (`LOG_ERROR`)
* **DEBUG**: Verbose debugging details (`LOG_DEBUG`)
* **TRACE**: Function- and file-aware tracing logs (`LOG_TRACE`)

### **Example**

```cpp
LOG_INFO << "Server started successfully";
LOG_WARNING << "High memory usage detected";
LOG_ERROR << "Failed to allocate buffer";
LOG_DEBUG << "Packet received: size = " << packet_size;
LOG_TRACE << "Path resolved from transcoder.";
```

## **3. Logging Categories**

Each subsystem/module has its own category tag to help filter logs effectively.

| **Category**          | **Use Case**                             |
| --------------------- | ---------------------------------------- |
| `DECODER_LOG`         | Audio decoding                           |
| `TRANSCODER_LOG`      | Audio transcoding                        |
| `LIBAV_LOG`           | FFmpeg/libav interaction                 |
| `AUDIO_LOG`           | Audio subsystem (rendering, buffering)   |
| `NETWORK_LOG`         | Networking routines (sockets, etc.)      |
| `TSFETCH_LOG`         | Transport stream fetching plugins        |
| `PLUGIN_LOG`          | Plugin management (loading/unloading)    |
| `HLS_LOG`             | HLS segmentation and manifesting         |
| `M3U8_PARSER_LOG`     | Parsing `.m3u8` playlists                |
| `CMD_LINE_PARSER_LOG` | Command-line option parsing              |
| `UNIX_LOG`            | Unix-specific backend tasks              |
| `DISPATCH_LOG`        | Dispatching stream worker sessions       |
| `SERVER_LOG`          | General server events                    |
| `SERVER_DWNLD_LOG`    | Handling downloads on the server         |
| `SERVER_UPLD_LOG`     | Handling uploads on the server           |
| `SERVER_EXTRACT_LOG`  | Archive extraction during upload         |
| `SERVER_VALIDATE_LOG` | Upload validation (size, checksum, etc.) |
| `OWNER_LOG`           | Owner session lifecycle (HLS, etc.)      |
| `CLIENT_LOG`          | Client-side stream playback and sync     |
| `FLAC_LOG`            | FLAC metadata parsing and streaming      |
| `NONE`                | Uncategorized (fallback/disabled)        |

### **Example**

```cpp
LOG_INFO << SERVER_LOG << "Client connected: ID = " << client_id;
LOG_ERROR << OWNER_LOG << "Invalid codec configuration!";
```

## **4. Synchronous vs Asynchronous Logging**

### **Synchronous Logging**

Used in the **main thread** or where blocking I/O is acceptable.

```cpp
LOG_INFO << "Starting server...";
LOG_ERROR << "File not found.";
```

### **Asynchronous Logging**

Used in **worker threads** or **performance-sensitive paths**.

```cpp
LOG_INFO_ASYNC << "Handling client request: ID = " << client_id;
LOG_TRACE_ASYNC << DISPATCH_LOG << "New worker for stream " << stream_id;
```

## **5. Log Formatting and Colors**

### Console Format

```
[2025-04-16 12:34:56.789] [INFO]    #DISPATCH_LOG Parsed stream header: version=3
```

### File Output

Same format but without ANSI color codes for clean grepping and reading.

### Color Mapping:

| **Level** | **Color** |
| ------ | --------- |
| TRACE     | Purple    |
| INFO      | Green     |
| WARNING   | Yellow    |
| ERROR     | Red       |
| DEBUG     | Blue      |

## **6. Utilities**

* `libwavy::log::flush_logs()` — Force flush log sinks to disk.
* `libwavy::log::set_log_level(log::SeverityLevel::DEBUG)` — Control global verbosity.
* `libwavy::log::strip_ansi()` — Remove ANSI color from strings (used internally).

## **7. Log File Location**

Log files are written to:

```bash
$HOME/.cache/wavy/logs/
```

With names like:

```bash
wavy_2025-04-16_12-30-00.log
```

Files rotate automatically.

## **8. Log Macros and Structured Logging API**

Wavy provides **templated, category-aware macros** for structured logging with compile-time format safety and sync/async mode support.

### Macros:

```cpp
libwavy::log::INFO<Tag>(fmt_string, args...);  // Sync
libwavy::log::INFO<Tag>(LogMode::Async, fmt, args...);  // Async
```

Same applies for:

* `ERROR<Tag>`
* `DBG<Tag>`
* `TRACE<Tag>`
* `WARN<Tag>`

### Example:

```cpp
libwavy::log::INFO<DISPATCH_LOG>("Parsed Audio-ID: {}", std::string(audio_id));
libwavy::log::ERROR<PLUGIN_LOG>(LogMode::Async, "Plugin failed: {}", plugin_name);

// Using NONE log category
INIT_WAVY_LOGGER();
lwlog::TRACE<_>("Hello there!");
```

### Compile-time Format Checking

All format macros use:

```cpp
static_assert(all_formattable_v<Args...>);
```

This ensures that all arguments are safely usable with `std::format`.

> [!NOTE]
>
> Convert non-formattable types like `std::filesystem::path`, `boost::string_view`, or `enum class` manually:
>
> ```cpp
> std::string(path)
> std::string_view(beast_sv)
> std::to_underlying(my_enum)
> ```
> Or wrap them:
>
> ```cpp
> template<typename SV>
> std::string to_std_string(SV s) {
>   return {s.data(), s.size()};
> }
> ```
>

### Enum `LogMode`

Use to toggle between synchronous and asynchronous logging per message:

```cpp
enum class LogMode {
  Sync,
  Async
};
```

### How It Works Internally

Every macro wraps:

```cpp
std::vformat(fmt, std::make_format_args(args...));
```

Then forwards it to the appropriate Boost.Log macro with a prepended category prefix via:

```cpp
log_prefix<Tag>()
```

### Format Safety Trait

```cpp
template <typename T>
struct is_formattable<T, std::void_t<decltype(std::formatter<std::remove_cvref_t<T>, char>{})>>
    : std::true_type {};
```

If any argument doesn't satisfy `std::formatter<T>`, a static assert will trigger.

## Notes

You might still require the **older** logging methods like `LOG_INFO`, etc. for special cases like **flushing** (which is NOT formattable):

```cpp
// !! This will NOT compile !!
lwlog::INFO<_>("\rI want flush {}", std::flush);

// == This WILL compile ==
LOG_INFO << "\rI want flush " << std::flush;
```

In special cases like the above **ONLY** is it permissible to utilize the `RAW LOG MACROS` instead of the newer log API.

## Summary

The Wavy logging system combines:

* ANSI-colored console logs
* Category-aware messages
* Thread-aware async logging
* Format-checked `std::format`-style APIs
* Auto-rotating log files

It’s **thread-safe**, **customizable**, and **designed for large modular C++ applications**.
