#pragma once

#include <string_view>

namespace libwavy::routes
{

inline constexpr char SERVER_PATH_OWNERS[]      = "/owners";
inline constexpr char SERVER_PATH_TOML_UPLOAD[] = "/upload";
inline constexpr char SERVER_PATH_AUDIO_INFO[]  = "/audio/info/";
inline constexpr char SERVER_PATH_PING[]        = "/ping";
inline constexpr char SERVER_PATH_DOWNLOAD[]    = "/download/<string>/<string>/<string>";
inline constexpr char SERVER_PATH_STREAM[]      = "/stream/<string>/<string>/<string>";
inline constexpr char SERVER_PATH_DELETE[]      = "/delete/<string>/<string>";

} // namespace libwavy::routes
