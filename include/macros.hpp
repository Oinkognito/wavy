#pragma once

#include <string>
#include <string_view>

#define TRANSPORT_STREAM_START_BYTE  0x47 // 0x47 (MPEG-TS sync byte)
#define WAVY_SERVER_PORT_NO          8080
#define WAVY_SERVER_AUDIO_SIZE_LIMIT 200 // in MiBs
#define WAVY_SERVER_PORT_NO_STR      "8080"

#define STRING_CONSTANTS(X)                                   \
  X(PLAYLIST_EXT, ".m3u8")                                    \
  X(PLAYLIST_GLOBAL_HEADER, "#EXTM3U")                        \
  X(MASTER_PLAYLIST, "index.m3u8")                            \
  X(TRANSPORT_STREAM_EXT, ".ts")                              \
  X(MP4_FILE_EXT, ".mp4")                                     \
  X(M4S_FILE_EXT, ".m4s")                                     \
  X(ZSTD_FILE_EXT, "zst")                                     \
  X(TOML_FILE_EXT, ".toml")                                   \
  X(FLAC_CODEC, "CODECS=\"fLaC\"")                            \
  X(COMPRESSED_ARCHIVE_EXT, ".tar.gz")                        \
  X(DISPATCH_ARCHIVE_REL_PATH, "payload")                     \
  X(DISPATCH_ARCHIVE_NAME, "hls_data.tar.gz")                 \
  X(CODEC_HLS_TIME_FIELD, "hls_time")                         \
  X(CODEC_HLS_LIST_SIZE_FIELD, "hls_list_size")               \
  X(CODEC_HLS_SEGMENT_FILENAME_FIELD, "hls_segment_filename") \
  X(CODEC_HLS_FLAGS_FIELD, "hls_flags")                       \
  X(CONTENT_TYPE_COMPRESSION, "application/gzip")             \
  X(CONTENT_TYPE_OCTET_STREAM, "application/octet-stream")    \
  X(PLAYLIST_VARIANT_TAG, "#EXT-X-STREAM-INF:")               \
  X(SERVER_PATH_HLS_CLIENTS, "/hls/clients")                  \
  X(SERVER_PATH_AUDIO_INFO, "/hls/audio-info/")               \
  X(SERVER_LOCK_FILE, "/tmp/hls_server.lock")                 \
  X(METADATA_FILE, "metadata.toml")                           \
  X(NETWORK_TEXT_DELIM, "\r\n\r\n")                           \
  X(SERVER_CERT, "server.crt")                                \
  X(SERVER_PRIVATE_KEY, "server.key")                         \
  X(SERVER_TEMP_STORAGE_DIR, "/tmp/hls_temp")                 \
  X(SERVER_STORAGE_DIR, "/tmp/hls_storage") // this will use /tmp of the server's filesystem

#define PROTOCOL_CONSTANTS(X)                                                              \
  X(SERVER_ERROR_404, "HTTP/1.1 404 Not Found\r\n\r\nFile not found")                      \
  X(SERVER_ERROR_500,                                                                      \
    "HTTP/1.1 500 Internal Server Error\r\n\r\nUnable to read file (or) File write error") \
  X(SERVER_ERROR_400, "HTTP/1.1 400 Bad Request\r\n\r\nInvalid request format")            \
  X(SERVER_ERROR_405, "HTTP/1.1 405 Method Not Allowed\r\n\r\n")                           \
  X(SERVER_ERROR_413, "HTTP/1.1 413 Payload Too Large\r\n\r\n")

namespace macros
{

#define DECLARE_STRING_VIEW(name, value) constexpr std::string_view name = value;
STRING_CONSTANTS(DECLARE_STRING_VIEW)
PROTOCOL_CONSTANTS(DECLARE_STRING_VIEW)
#undef DECLARE_STRING_VIEW

// Convert string_view to string using a function (avoiding constexpr std::string)
inline auto to_string(std::string_view sv) -> std::string { return std::string(sv); }

} // namespace macros
