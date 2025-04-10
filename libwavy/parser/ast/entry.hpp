#pragma once
/************************************************
 * Wavy Project - High-Fidelity Audio Streaming
 * ---------------------------------------------
 * 
 * Copyright (c) 2025 Oinkognito
 * All rights reserved.
 * 
 * This source code is part of the Wavy project, an advanced
 * local networking solution for high-quality audio streaming.
 * 
 * License:
 * This software is licensed under the BSD-3-Clause License.
 * You may use, modify, and distribute this software under
 * the conditions stated in the LICENSE file provided in the
 * project root.
 * 
 * Warranty Disclaimer:
 * This software is provided "AS IS," without any warranties
 * or guarantees, either expressed or implied, including but
 * not limited to fitness for a particular purpose.
 * 
 * Contributions:
 * Contributions to this project are welcome. By submitting 
 * code, you agree to license your contributions under the 
 * same BSD-3-Clause terms.
 * 
 * See LICENSE file for full details.
 ************************************************/

#include <map>
#include <optional>
#include <string>
#include <vector>

// Structures that hold and create the AST for playlist files and its contents

namespace libwavy::hls::parser::ast
{

struct VariantStream
{
  int                        bitrate;    // BANDWIDTH or AVERAGE-BANDWIDTH
  std::string                uri;        // Path to the bitrate playlist
  std::optional<std::string> resolution; // Optional RESOLUTION=
  std::optional<std::string> codecs;     // Optional CODECS=
};

struct Segment
{
  float       duration = 0.0f; // #EXTINF: duration
  std::string uri;             // URI of the media segment
};

struct MediaPlaylist
{
  int                        bitrate; // Derived from the master playlist
  std::optional<std::string> map_uri;
  std::vector<Segment>       segments; // .ts or .m4s segments
};

struct MasterPlaylist
{
  std::vector<VariantStream>   variants;        // List of bitrate playlists
  std::map<int, MediaPlaylist> media_playlists; // Filled after parsing children
};

} // namespace libwavy::hls::parser::ast
