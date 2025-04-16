# Logging in Wavy (`logger.hpp`)

The `logger.hpp` file provides a structured logging system for Wavy, leveraging **Boost.Log** for both console and file-based logging. It includes features such as **color-coded severity levels**, **timestamped logs**, **per-component log tags**, and **support for multi-threaded logging** with thread-aware macros.

## **1. Initialization**
Before using the logger, call:
```cpp
libwavy::log::init_logging();
```

This initializes:
- Console and file sinks
- Directory creation at `$HOME/.cache/wavy/logs`
- ANSI-colored console output
- ANSI-stripped file output
- Timestamped log entries

## **2. Logging Severity Levels**
Five severity levels are supported:
- **INFO**: General status messages (`LOG_INFO`)
- **WARNING**: Potential issues (`LOG_WARNING`)
- **ERROR**: Critical errors (`LOG_ERROR`)
- **DEBUG**: Verbose debugging details (`LOG_DEBUG`)
- **TRACE**: Function- and file-aware tracing logs (`LOG_TRACE`)

### **Example Usage**
```cpp
LOG_INFO << "Server started successfully";
LOG_WARNING << "High memory usage detected";
LOG_ERROR << "Failed to allocate buffer";
LOG_DEBUG << "Packet received: size = " << packet_size;
LOG_TRACE << "Received path from transcoder..";
```

> [!NOTE]
> 
> Use `LOG_TRACE` or `LOG_ERROR` to automatically include file, line, and function name.

## **3. Logging Categories**
Each module has its own category tag to aid with filtering or grep-ing logs.

| **Category**             | **Use Case**                        |
|--------------------------|--------------------------------------|
| `DECODER_LOG`            | Audio decoding                      |
| `TRANSCODER_LOG`         | Audio transcoding                   |
| `LIBAV_LOG`              | FFmpeg/libav related                |
| `AUDIO_LOG`              | Audio subsystem in general          |
| `NETWORK_LOG`            | Networking routines                 |
| `TSFETCH_LOG`            | Transport stream fetching plugins   |
| `PLUGIN_LOG`             | Plugin loading or unloading         |
| `HLS_LOG`                | HLS segmenting and manifesting      |
| `M3U8_PARSER_LOG`        | M3U8 playlist parsing               |
| `UNIX_LOG`               | Unix-specific tasks                 |
| `DISPATCH_LOG`           | Dispatch stream workers             |
| `SERVER_LOG`             | General server events               |
| `SERVER_DWNLD_LOG`       | Server-side download handling       |
| `SERVER_UPLD_LOG`        | Server-side upload handling         |
| `SERVER_EXTRACT_LOG`     | Archive extraction phase            |
| `SERVER_VALIDATE_LOG`    | Uploaded data validation            |
| `OWNER_LOG`              | Owner responsibilities (HLS, etc.) |
| `RECEIVER_LOG`           | Receiving data streams              |

### **Example**
```cpp
LOG_INFO << SERVER_LOG << "Client connected: ID = " << client_id;
LOG_ERROR << OWNER_LOG << "Invalid codec configuration!";
```

## **4. Synchronous vs Asynchronous Logging**
### **Synchronous**
Use when logs are emitted from the **main thread** or when **performance is not a concern**:
```cpp
LOG_INFO << "Starting server...";
LOG_ERROR << "File not found.";
```

### **Asynchronous**
Use when logging inside **worker threads** or **performance-critical** paths:
```cpp
LOG_INFO_ASYNC << "Handling request from client " << client_id;
LOG_TRACE_ASYNC << DISPATCH_LOG << "New worker for stream " << stream_id;
```

Async macros include:
- `THREAD_ID`: Unique thread identifier
- `_TRACE_BACK_`: Adds `[file:line - function]`

#### When to Use Asynchronous Logging:
| Use Case                                     | Logging Type   |
|---------------------------------------------|----------------|
| Worker threads / thread pool tasks          | `LOG_*_ASYNC`  |
| Debugging multi-threaded logic               | `LOG_DEBUG_ASYNC`, `LOG_TRACE_ASYNC` |
| Logging during packet decoding / processing | `LOG_INFO_ASYNC` or `LOG_ERROR_ASYNC` |

## **5. Log Formatting and Colors**
### 🖥 Console Output Format:
```
[BOLD][YYYY-MM-DD HH:MM:SS.mmm] [LEVEL] [CATEGORY] Message
```

### File Output Format:
Same as above, but **without ANSI color codes**, for cleaner grep and IDE viewing.

### Color Mapping:
| **Level** | **Color**   | **Macro**         |
|-----------|-------------|-------------------|
| TRACE     | Purple      | `LOG_TRACE`       |
| INFO      | Green       | `LOG_INFO`        |
| WARNING   | Yellow      | `LOG_WARNING`     |
| ERROR     | Red         | `LOG_ERROR`       |
| DEBUG     | Blue        | `LOG_DEBUG`       |

### **Example Console Output**
```
[2025-04-16 12:34:56.789] [INFO]    #SERVER_LOG Client connected: ID = 12345
[2025-04-16 12:34:56.900] [WARNING] #OWNER_LOG Encoding delay detected
[2025-04-16 12:34:57.101] [ERROR]   #DISPATCH_LOG Failed to open socket
[2025-04-16 12:34:57.302] [TRACE]   #TSFETCH_LOG Entering retry loop...
```

## **6. Utilities**
- `libwavy::log::flush_logs()` — Manually flush the log sinks.
- `libwavy::log::set_log_level(libwavy::log::SeverityLevel::DEBUG)` — Set minimum log level.
- `libwavy::log::strip_ansi()` — Remove color codes from strings (used internally for file logging).

## **7. Extras**
- ANSI color definitions match **Gruvbox** palette.
- Log files are stored in:  
  ```bash
  ~/.cache/wavy/logs/
  ```
  with auto-rotating names like:  
  `wavy_2025-04-16_12-30-00.log`
