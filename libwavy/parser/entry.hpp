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

#include <algorithm>
#include <autogen/config.h>
#include <filesystem>
#include <fstream>
#include <libwavy/common/api/entry.hpp>
#include <libwavy/common/types.hpp>
#include <libwavy/log-macros.hpp>
#include <libwavy/parser/ast/entry.hpp>
#include <libwavy/parser/macros.hpp>
#include <string>
#include <string_view>
#include <type_traits>

namespace fs = std::filesystem;
using M3U8   = libwavy::log::M3U8_PARSER;

// Template-based playlist parser that can handle both file paths and string content
//
// There are some caveats to this method, but for the way
// that Wavy creates these playlists, this should work
//
// So yea not a universal playlist parser but works for us.

namespace libwavy::hls::parser
{

// Concept to distinguish between file path and content strings
template <typename T>
concept StringLike =
  std::is_convertible_v<T, std::string> || std::is_convertible_v<T, std::string_view>;

class WAVY_API M3U8Parser
{
public:
  // Parse master playlist from either a file path or direct content
  template <StringLike T>
  static auto parseMasterPlaylist(const T& source, std::optional<AbsPath> base_path = std::nullopt)
    -> ast::MasterPlaylist
  {
    if constexpr (std::is_same_v<std::decay_t<T>, std::string> ||
                  std::is_same_v<std::decay_t<T>, std::string_view>)
    {
      // If a base path is not provided, use current directory
      AbsPath base = base_path.value_or(".");
      log::DBG<M3U8>("Using provided base path '{}' for master playlist.", base.str());
      return parseMaster(std::string(source), base);
    }
    else
    {
      // Treat as file path
      std::ifstream file(source);
      if (!file)
      {
        log::ERROR<M3U8>("Cannot open master playlist: {}", source);
        return {};
      }

      std::stringstream ss;
      ss << file.rdbuf();

      AbsPath base;
      if (base_path)
      {
        base = *base_path;
      }
      else
      {
        // Derive base path from the file path
        fs::path derived_base = fs::path(source).parent_path();
        base                  = derived_base.string();
      }

      log::DBG<M3U8>("Using base path '{}' for master playlist.", base);
      return parseMaster(ss.str(), base);
    }
  }

  // Parse media playlist from either a file path or direct content
  template <StringLike T>
  static auto parseMediaPlaylist(const T& source, int bitrate, const Directory& base_dir = ".")
    -> ast::MediaPlaylist
  {
    if constexpr (std::is_same_v<std::decay_t<T>, std::string> ||
                  std::is_same_v<std::decay_t<T>, std::string_view>)
    {
      // Direct content
      fs::path base_path = fs::path(base_dir).lexically_normal();
      log::DBG<M3U8>("Using base path for media segments: {}", base_path.string());
      return parseMedia(std::string(source), bitrate, base_path.string());
    }
    else
    {
      // File path
      std::ifstream file(source);
      if (!file)
      {
        log::ERROR<M3U8>("Cannot open media playlist file: {}", source);
        return {};
      }

      std::stringstream buffer;
      buffer << file.rdbuf();

      fs::path base_path = fs::path(base_dir).lexically_normal();
      log::DBG<M3U8>("Using base path for media segments: '{}'", base_path.string());

      return parseMedia(buffer.str(), bitrate, base_path.string());
    }
  }

private:
  static auto parseMaster(const PlaylistData& content, const AbsPath& base_path)
    -> ast::MasterPlaylist
  {
    ast::MasterPlaylist               master;
    std::optional<ast::VariantStream> pending;

    std::istringstream ss(content);
    std::string        line;

    while (std::getline(ss, line))
    {
      std::string_view sv = trim(line);
      if (sv.starts_with(macro::EXT_X_STREAM_INF))
      {
        pending = parseVariantInfo(sv);
      }
      else if (!sv.empty() && sv[0] != '#' && pending)
      {
        fs::path uri_path = fs::path(base_path) / std::string(sv);
        log::DBG<M3U8>("Found URI Path: '{}'", uri_path.string());
        pending->uri = uri_path.lexically_normal().string();
        master.variants.push_back(*pending);
        pending.reset();
      }
    }
    return master;
  }

  static auto parseMedia(const PlaylistData& content, int bitrate, const Directory& base_path)
    -> ast::MediaPlaylist
  {
    ast::MediaPlaylist media;
    media.bitrate = bitrate;

    std::istringstream   ss(content);
    std::string          line;
    std::optional<float> pending_duration;
    bool                 map_found = false;

    while (std::getline(ss, line))
    {
      std::string_view sv = trim(line);

      if (sv.empty())
        continue;

      if (sv.starts_with(macro::EXT_X_MAP))
      {
        size_t uri_pos = sv.find(macro::URI);
        if (uri_pos != std::string_view::npos)
        {
          auto uri_sub = sv.substr(uri_pos + 4);
          if (!uri_sub.empty() && uri_sub.front() == '"')
          {
            uri_sub.remove_prefix(1);
            size_t quote_pos = uri_sub.find('"');
            if (quote_pos != std::string_view::npos)
            {
              std::string map_uri   = std::string(uri_sub.substr(0, quote_pos));
              fs::path    full_path = fs::path(base_path) / map_uri;
              media.map_uri         = full_path.lexically_normal().string();
              map_found             = true;
              log::DBG<M3U8>("Found EXT-X-MAP URI: {}", media.map_uri.value());
            }
          }
        }
      }
      else if (sv.starts_with(macro::EXTINF))
      {
        auto duration_str = sv.substr(8);
        auto comma_pos    = duration_str.find(',');
        if (comma_pos != std::string_view::npos)
          duration_str.remove_suffix(duration_str.size() - comma_pos);

        try
        {
          float duration   = std::stof(std::string(duration_str));
          pending_duration = duration;
          log::DBG<M3U8>("Parsed EXTINF: duration={}", duration);
        }
        catch (...)
        {
          log::WARN<M3U8>("Failed to parse EXTINF duration: {}", duration_str);
          pending_duration.reset(); // Defensive
        }
      }
      else if (sv.front() != '#')
      {
        if (pending_duration.has_value())
        {
          std::string seg_uri   = std::string(sv);
          fs::path    full_path = fs::path(base_path) / seg_uri;
          std::string norm_uri  = full_path.lexically_normal().string();

          media.segments.emplace_back(ast::Segment{.duration = *pending_duration, .uri = norm_uri});

          log::DBG<M3U8>("Added Segment: duration={}, uri={}", *pending_duration, norm_uri);
          pending_duration.reset();
        }
        else
        {
          log::WARN<M3U8>("Skipping segment without preceding EXTINF: {}", sv);
        }
      }
    }

    if (!map_found)
    {
      log::INFO<M3U8>("No EXT-X-MAP field found in media playlist: '{}'", base_path);
    }

    log::DBG<M3U8>("Parsed media playlist with {} segments.", media.segments.size());
    return media;
  }

  static auto trim(std::string_view s) -> std::string_view
  {
    size_t start = s.find_first_not_of(" \t\r\n");
    size_t end   = s.find_last_not_of(" \t\r\n");
    return (start == std::string_view::npos) ? "" : s.substr(start, end - start + 1);
  }

  static auto parseVariantInfo(std::string_view line) -> ast::VariantStream
  {
    ast::VariantStream vs{};
    auto extract = [](std::string_view src, std::string_view key) -> std::optional<std::string>
    {
      size_t pos = src.find(key);
      if (pos == std::string_view::npos)
        return std::nullopt;
      size_t start = pos + key.size();
      size_t end   = src.find_first_of(",\r\n", start);
      return std::string(src.substr(start, end - start));
    };

    if (auto bw = extract(line, macro::AVERAGE_BANDWIDTH);
        bw && std::ranges::all_of(*bw, ::isdigit))
      vs.bitrate = std::stoi(*bw);
    else if (auto bw2 = extract(line, macro::BANDWIDTH);
             bw2 && std::ranges::all_of(*bw2, ::isdigit))
      vs.bitrate = std::stoi(*bw2);

    vs.resolution = extract(line, macro::RESOLUTION);
    vs.codecs     = extract(line, macro::CODECS);

    return vs;
  }
};

template <typename T> inline void printAST(const T& node)
{
  if constexpr (std::is_same_v<T, ast::MasterPlaylist>)
  {
    log::INFO<M3U8>("=== Master Playlist AST ===");
    log::INFO<M3U8>("Master AST represents {} node: ", node.variants.size());
    for (const auto& variant : node.variants)
    {
      log::INFO<M3U8>("  - Bitrate: {}", variant.bitrate);
      if (variant.resolution)
        log::INFO<M3U8>("    Resolution: {}", *variant.resolution);
      if (variant.codecs)
        log::INFO<M3U8>("    Codecs: {}", *variant.codecs);
      log::INFO<M3U8>("    URI: {}", variant.uri);
    }
  }
  else if constexpr (std::is_same_v<T, ast::MediaPlaylist>)
  {
    log::INFO<M3U8>("=== Media Playlist AST ===");
    log::INFO<M3U8>("AST represents {} segments.", node.segments.size());
    log::INFO<M3U8>("Bitrate: {}", node.bitrate);
    for (const auto& segment : node.segments)
    {
      log::INFO<M3U8>("  - Duration: {}", segment.duration);
      log::INFO<M3U8>("    URI: {}", segment.uri);
    }

    if (node.map_uri)
      log::INFO<M3U8>("Init segment URI: {}", *node.map_uri);
  }
  else
  {
    static_assert(sizeof(T) == 0, "printAST not implemented for this type.");
  }
}
} // namespace libwavy::hls::parser
