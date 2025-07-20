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

#include <string_view>

#define HLS_KEYWORDS(MACRO)                      \
  MACRO(CODECS, "CODECS=")                       \
  MACRO(BANDWIDTH, "BANDWIDTH=")                 \
  MACRO(URI, "URI=")                             \
  MACRO(AVERAGE_BANDWIDTH, "AVERAGE-BANDWIDTH=") \
  MACRO(RESOLUTION, "RESOLUTION=")               \
  MACRO(EXT_X_STREAM_INF, "#EXT-X-STREAM-INF:")  \
  MACRO(EXT_X_MAP, "#EXT-X-MAP:")                \
  MACRO(EXTINF, "#EXTINF:")

namespace libwavy::hls::parser::macro
{

#define DECLARE_KEYWORD(name, str) inline constexpr std::string_view name = str;
HLS_KEYWORDS(DECLARE_KEYWORD)
#undef DECLARE_KEYWORD

} // namespace libwavy::hls::parser::macro
