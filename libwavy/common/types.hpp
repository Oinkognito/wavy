#pragma once

// Contains typedefs for the entire project (aside from Audio related ones in global state)

#include <cstdint>
#include <string>
#include <vector>

//[ Int types ]//
using i8   = std::int8_t;
using i16  = std::int16_t;
using i32  = std::int32_t;
using i64  = std::int64_t;
using uint = unsigned int;
using ui8  = std::uint8_t;

//[ NETWORKING DEFS ]//
using IPAddr      = std::string;       // IP Address typedef (typically for server ip)
using Nickname    = std::string;       // Nickname to be stored in the server
using PortNo      = int;               // Port number of the server
using NetMethods  = const std::string; // Methods like "GET", "POST", etc.
using NetTarget   = const std::string; // Requested target to check in server
using NetResponse = std::string;       // Response from server

//[ DIRECTORY AND PATHS DEFS ]//
using Directory = std::string; // Directory represented as a string
using RelPath   = std::string; // Relative Path as a string (need not be of a file)
using AbsPath   = std::string; // Absolute Path as a string (need not be of a file)
using FileName =
  std::string; // Just the filename as a string (only use this when it is just a file name)
using DirPathHolder = std::
  string; // When creating members of a class that hold dir / path / file name info at all, use this holder

//[ DIRECTORY AND PATHS C-STYLE DEFS ]//
using CStrDirectory = const char*; // Directory represented as const char*
using CStrRelPath   = const char*; // Relative Path as const char*
using CStrAbsPath   = const char*; // Absolute Path as const char*
using CStrFileName  = const char*; // Just the filename as const char*

// NOTE: CStrRelPath and RelPath have the leisure of storing EVEN the absolute path, but NOT vice-versa!
//
// Only use AbsPath and CStrAbsPath when you are ABSOLUTELY sure that the data stored is an absolute path!!

//[ PLAYLIST CONTENT ]//
using PlaylistData = std::string; // The playlist (.m3u8) content stored here

//[ SERVER STORAGE DEFS ]//
using StorageOwnerID = std::string;
using StorageAudioID = std::string;

//[ AUDIO BACKEND PLUGIN DEFS ]//
using AudioBackendPluginName               = const char*;
using AudioByte                            = ui8;
using AudioBuffer                          = std::vector<AudioByte>;
using AudioOffset                          = std::size_t;
using AudioChunk                           = std::size_t;
using SampleSize                           = std::size_t;
using ByteCount                            = size_t;
using AudioStreamIdx                       = int;
using AudioStreamIdxIter                   = uint;
inline constexpr SampleSize BytesPerSample = sizeof(float) * 2; // stereo float
