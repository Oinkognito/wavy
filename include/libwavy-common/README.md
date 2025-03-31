# **LIBWAVY-COMMON**

Contains the common utilities of the entire project:

1. `common.h`: For `ZSTD` compression and decompression and main C FFI source.
2. `logger.hpp`: The entire project's logging source with Boost logs
3. `macros.hpp`: Self explanatory all global macros are defined here
4. `state.hpp`: For now just has the global struct of transport segments for decoding audio without file I/O.

## Logging in Wavy (`logger.hpp`)

The `logger.hpp` file provides a structured logging system for Wavy, leveraging **Boost.Log** for both console and file-based logging. It includes features such as **color-coded severity levels**, **timestamped logs**, and **support for multi-threaded logging**.

## **1. Initialization**
Before using the logger, call:
```cpp
logger::init_logging();
```
This initializes the logging system and configures console output formatting.

## **2. Logging Severity Levels**
Four severity levels are available:
- **INFO**: General status messages (`LOG_INFO`)
- **WARNING**: Potential issues (`LOG_WARNING`)
- **ERROR**: Critical errors that need immediate attention (`LOG_ERROR`)
- **DEBUG**: Detailed debugging information (`LOG_DEBUG`)

### **Example Usage**
```cpp
LOG_INFO << "Server started successfully";
LOG_WARNING << "High memory usage detected";
LOG_ERROR << "Failed to allocate buffer";
LOG_DEBUG << "Packet received: size = " << packet_size;
```

## **3. Logging Categories**
Each component of Wavy has a **log category**, making it easier to filter logs. The following categories are defined:

| **Category**        | **Use Case** |
|---------------------|-------------|
| `DECODER_LOG`      | Logs related to audio decoding |
| `ENCODER_LOG`      | Logs related to audio encoding |
| `DISPATCH_LOG`     | Logs related to dispatching streams |
| `SERVER_LOG`       | General server logs |
| `SERVER_DWNLD_LOG` | Server handling downloads |
| `SERVER_UPLD_LOG`  | Server handling uploads |
| `SERVER_EXTRACT_LOG` | Extracting uploaded archive files |
| `SERVER_VALIDATE_LOG` | Validation of uploaded files |
| `RECEIVER_LOG`     | Logs related to receiving streams |

### **Example Usage**
```cpp
LOG_INFO << SERVER_LOG << "Client connected: ID = " << client_id;
LOG_ERROR << ENCODER_LOG << "Encoding failed due to invalid codec settings";
```

## **4. Synchronous vs Asynchronous Logging**
### **Synchronous Logs**
- Used when logging operations **do not** affect performance critically.
- Safe to use in single-threaded contexts.

**Example**
```cpp
LOG_INFO << "Server is running on port " << port;
LOG_ERROR << "File not found: " << filename;
```

### **Asynchronous Logs**
- Used in **multi-threaded** environments where logs originate from worker threads.
- Includes the **thread ID** to track logs from different workers.
- Helps in debugging concurrency-related issues.

**Example**
```cpp
LOG_INFO_ASYNC << "Handling request from client " << client_id;
LOG_DEBUG_ASYNC << "Decoder thread started for stream ID " << stream_id;
```

#### **When to Use Asynchronous Logging**
Use **asynchronous logging** when:
- Logging in parallel threads (e.g., worker threads handling audio packets).
- Logging events that occur **frequently**, to reduce contention on `std::cout`.
- Debugging issues in **multi-threaded execution**.

Use **synchronous logging** when:
- Logging in the **main thread** or non-performance-critical sections.
- Logging **important** messages that should be processed immediately.
- Writing logs that require **immediate visibility** without buffering.

## **5. Log Formatting and Colors**
The log output is structured as follows:
```
[BOLD][HH:MM:SS.mmm] [LEVEL] [CATEGORY] Message
```
**Example**
```
[12:34:56.789] [INFO]    #SERVER_LOG Client connected: ID = 12345
[12:34:56.900] [WARNING] #ENCODER_LOG Encoding delay detected
[12:34:57.101] [ERROR]   #DISPATCH_LOG Failed to open network socket
```
- **INFO** logs are **green**.
- **WARNING** logs are **yellow**.
- **ERROR** logs are **red**.
- **DEBUG** logs are **blue**.

