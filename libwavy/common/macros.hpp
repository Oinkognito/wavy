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

#include <string>
#include <string_view>

enum Macros
{
  TRANSPORT_STREAM_START_BYTE  = 0x47, // 0x47 (MPEG-TS sync byte)
  WAVY_SERVER_PORT_NO          = 8080,
  WAVY_SERVER_AUDIO_SIZE_LIMIT = 200 // in MiBs
};

#define WAVY_SERVER_PORT_NO_STR "8080"
/// Basic string for Carriage Return Line Feed (CRLF x 2)
#define CRLF2 "\r\n\r\n"
/// Basic string for Carriage Return Line Feed (CRLF)
#define CRLF "\r\n"

#define WAVY_RET_SUC   0
#define WAVY_RET_FAIL  1
#define WAVY_RET_UNDEF -1

#define WAVY__SAFE_MULTIPLY(a, b, result)                               \
  do                                                                    \
  {                                                                     \
    using safe_type = long; /* Ensuring safe promotion */               \
    if ((a) > 0 && (b) > (std::numeric_limits<safe_type>::max() / (a))) \
    {                                                                   \
      throw std::overflow_error("Multiplication overflow detected!");   \
    }                                                                   \
    (result) = static_cast<safe_type>(a) * static_cast<safe_type>(b);   \
  } while (0)

#define WAVY__TYPE_NAME(var) typeid(var).name()
#define WAVY__IS_SAME(A, B)  std::is_same<A, B>::value

#define WAVY__UNREACHABLE()                          \
  do                                                 \
  {                                                  \
    ::wavy::unreachable_reached(__FILE__, __LINE__); \
    std::abort();                                    \
  } while (0)

#define WAVY__NOT_IMPLEMENTED()                            \
  do                                                       \
  {                                                        \
    ::wavy::not_implemented(__FILE__, __LINE__, __func__); \
    std::abort();                                          \
  } while (0)

#define STRING_CONSTANTS(X)                                   \
  /* File Extensions */                                       \
  X(PLAYLIST_EXT, ".m3u8")                                    \
  X(TRANSPORT_STREAM_EXT, ".ts")                              \
  X(MP4_FILE_EXT, ".mp4")                                     \
  X(M4S_FILE_EXT, ".m4s")                                     \
  X(MP3_FILE_EXT, ".mp3")                                     \
  X(FLAC_FILE_EXT, ".flac")                                   \
  X(ZSTD_FILE_EXT, "zst")                                     \
  X(OWNER_FILE_EXT, ".owner")                                 \
  X(TOML_FILE_EXT, ".toml")                                   \
  X(COMPRESSED_ARCHIVE_EXT, ".tar.gz")                        \
                                                              \
  /* Playlist Content */                                      \
  X(PLAYLIST_GLOBAL_HEADER, "#EXTM3U")                        \
  X(MASTER_PLAYLIST, "index.m3u8")                            \
  X(MASTER_PLAYLIST_HEADER, "#EXTM3U\n#EXT-X-VERSION:3\n")    \
  X(PLAYLIST_VARIANT_TAG, "#EXT-X-STREAM-INF:")               \
                                                              \
  /* Codec Identifiers */                                     \
  X(FLAC_CODEC, "CODECS=\"fLaC\"")                            \
  X(MP3_CODEC, "CODECS=\"mp4a.40.2\"")                        \
                                                              \
  /* Segment Identifiers */                                   \
  X(MPEG_TS, "mpegts")                                        \
  X(MP4_TS, "mp4")                                            \
                                                              \
  /* HLS Codec Options */                                     \
  X(CODEC_HLS_TIME_FIELD, "hls_time")                         \
  X(CODEC_HLS_LIST_SIZE_FIELD, "hls_list_size")               \
  X(CODEC_HLS_SEGMENT_FILENAME_FIELD, "hls_segment_filename") \
  X(CODEC_HLS_FLAGS_FIELD, "hls_flags")                       \
                                                              \
  /* Server File & Metadata */                                \
  X(DISPATCH_ARCHIVE_REL_PATH, "wavy-owner-payload")          \
  X(DISPATCH_ARCHIVE_NAME, "hls_data.tar.gz")                 \
  X(METADATA_FILE, "metadata.toml")                           \
                                                              \
  /* Content Types */                                         \
  X(CONTENT_TYPE_COMPRESSION, "application/gzip")             \
  X(CONTENT_TYPE_OCTET_STREAM, "application/octet-stream")    \
  X(CONTENT_TYPE_JSON, "application/json")                    \
  X(CONTENT_TYPE_GZIP, "application/gzip")                    \
                                                              \
  /* Server Endpoints */                                      \
  X(SERVER_PATH_HLS_OWNERS, "/hls/owners")                    \
  X(SERVER_PATH_TOML_UPLOAD, "/toml/upload")                  \
  X(SERVER_PATH_AUDIO_INFO, "/hls/audio-info/")               \
  X(SERVER_PATH_PING, "/hls/ping")                            \
                                                              \
  /* Locking & Protocol Helpers */                            \
  X(SERVER_LOCK_FILE, "/tmp/wavy_server.lock")                \
  X(NETWORK_TEXT_DELIM, "\r\n\r\n")                           \
                                                              \
  /* Certificate & Key Files */                               \
  X(SERVER_CERT, "server.crt")                                \
  X(SERVER_PRIVATE_KEY, "server.key")                         \
                                                              \
  /* Directories */                                           \
  X(SERVER_TEMP_STORAGE_DIR, "/tmp/wavy_temp")                \
  X(SERVER_STORAGE_DIR, "/tmp/wavy_storage") // tmp of server filesystem

#define PROTOCOL_CONSTANTS(X)                                                               \
  X(SERVER_ERROR_400, "HTTP/1.1 400 Bad Request" CRLF2 "Invalid request format")            \
  X(SERVER_ERROR_401, "HTTP/1.1 401 Authentication Error" CRLF2)                            \
  X(SERVER_ERROR_404, "HTTP/1.1 404 Not Found" CRLF2 "File not found")                      \
  X(SERVER_ERROR_405, "HTTP/1.1 405 Method Not Allowed" CRLF2)                              \
  X(SERVER_ERROR_413, "HTTP/1.1 413 Payload Too Large" CRLF2)                               \
  X(SERVER_ERROR_500,                                                                       \
    "HTTP/1.1 500 Internal Server Error" CRLF2 "Unable to read file (or) File write error") \
  X(SERVER_PONG_MSG, "HTTP/1.1 200 OK" CRLF "Content-Type: text/plain" CRLF2 "pong")

namespace macros
{

#define DECLARE_STRING_VIEW(name, value) constexpr std::string_view name = value;
STRING_CONSTANTS(DECLARE_STRING_VIEW)
PROTOCOL_CONSTANTS(DECLARE_STRING_VIEW)
#undef DECLARE_STRING_VIEW

// Convert string_view to string using a function (avoiding constexpr std::string)
inline auto to_string(std::string_view sv) -> std::string { return std::string(sv); }
inline auto to_cstr(std::string_view sv) -> const char*
{
  thread_local std::string buffer;
  buffer = sv;
  return buffer.c_str();
}

} // namespace macros
