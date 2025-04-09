# M3U8Parser Overview

The `M3U8Parser` is a lightweight parser for HTTP Live Streaming (HLS) playlists. It supports parsing both master playlists (`.m3u8` files describing variant streams) and media playlists (`.m3u8` files describing actual audio segment sequences). This parser is intended for internal use within the `libwavy` project to construct a structured Abstract Syntax Tree (AST) representation from raw M3U8 content.

## Purpose

This parser enables:

- Loading and interpreting HLS master playlists to determine available bitrate/resolution variants.
- Resolving relative or absolute URIs for media playlists and segments.
- Parsing segment durations and optional initialization maps (`#EXT-X-MAP`) from media playlists.
- Representing the parsed playlist structure in an internal AST format suitable for inspection, manipulation, or feeding into audio streaming workflows.

## Features

- **Support for both file-based and in-memory strings**: Works with either file paths or raw M3U8 playlist content.
- **Boost Filesystem Integration**: Uses `boost::filesystem` to resolve base paths and normalize URIs.
- **Logging**: Extensively logs parsing actions and errors for easy debugging and inspection.
- **Templated Design**: Uses C++20 concepts to distinguish between file input and string input at compile time.
- **AST Representation**: Cleanly separates the parsing logic from internal data structures (`ast::MasterPlaylist` and `ast::MediaPlaylist`).

## Templated Parsing Logic

A standout design choice in `M3U8Parser` is the use of **template-based interfaces** to handle both raw string content and file paths using a single unified function.

```cpp
template <StringLike T>
static auto parseMasterPlaylist(const T& source, std::optional<std::string> base_path = std::nullopt)
  -> ast::MasterPlaylist;
```

This template uses a **C++20 concept** called `StringLike` to determine if `T` is a string-like type (`std::string`, `std::string_view`, etc.).

- If the input is string content: it directly parses it using `parseMaster(...)` or `parseMedia(...)`.
- If the input is a file path: it opens the file, reads its contents, derives a base path, and continues parsing.

This makes the API highly flexible and easy to use:

```cpp
// Example: Parsing from raw string
std::string master_m3u8 = "#EXTM3U\n#EXT-X-STREAM-INF:BANDWIDTH=128000\n128k.m3u8\n";
auto master_ast = M3U8Parser::parseMasterPlaylist(master_m3u8);

// Example: Parsing from file path
auto master_ast = M3U8Parser::parseMasterPlaylist("path/to/master.m3u8");
```

This technique ensures that `libwavy` can use the parser in various contexts, such as:

- Reading from the filesystem.
- Receiving playlists over the network.
- Embedding test playlist data directly in source code.

## AST Dumping for Debugging

To help visualize the internal AST, the parser includes a `printAST` helper function that prints out the contents of the parsed structure with `LOG_INFO`:

```cpp
printAST(master_ast);
printAST(media_ast);
```

## Caveats

This parser is tailored for the specific M3U8 format and structure generated or consumed by `libwavy`. It **DOES NOT** handle all edge cases in HLS specifications and is not intended for general-purpose use.
