#pragma once

#include <string>
#include <string_view>

#define STRING_CONSTANTS(X)                                \
  X(PLAYLIST_EXT, ".m3u8")                                 \
  X(MASTER_PLAYLIST, "index.m3u8")                         \
  X(TRANSPORT_STREAM_EXT, ".ts")                           \
  X(COMPRESSED_ARCHIVE_EXT, ".tar.gz")                     \
  X(DISPATCH_ARCHIVE_NAME, "hls_data.tar.gz")              \
  X(CONTENT_TYPE_COMPRESSION, "application/gzip")          \
  X(CONTENT_TYPE_OCTET_STREAM, "application/octet-stream") \
  X(PLAYLIST_HEADER, "#EXT-X-STREAM-INF")                  \
  X(SERVER_CERT, "server.crt")                             \
  X(SERVER_PRIVATE_KEY, "server.key")                      \
  X(SERVER_TEMP_STORAGE_DIR, "/tmp/hls_temp")              \
  X(SERVER_STORAGE_DIR, "/tmp/hls_storage") // this will use /tmp of the server's filesystem

namespace macros
{

#define DECLARE_STRING_VIEW(name, value) constexpr std::string_view name = value;
STRING_CONSTANTS(DECLARE_STRING_VIEW)
#undef DECLARE_STRING_VIEW

// Convert string_view to string using a function (avoiding constexpr std::string)
inline auto to_string(std::string_view sv) -> std::string { return std::string(sv); }

} // namespace macros
