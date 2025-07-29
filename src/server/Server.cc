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

#include <libwavy/common/api/entry.hpp>
#include <libwavy/log-macros.hpp>
#include <libwavy/server/server.hpp>

using SExtract = libwavy::log::SERVER_EXTRACT;
using SDwnld   = libwavy::log::SERVER_DWNLD;
using SUpload  = libwavy::log::SERVER_UPLD;
using SValid   = libwavy::log::SERVER_VALIDATE;

namespace libwavy::server::helpers
{

auto is_valid_extension(const FileName& filename) -> bool
{
  return filename.ends_with(macros::PLAYLIST_EXT) ||
         filename.ends_with(macros::TRANSPORT_STREAM_EXT) ||
         filename.ends_with(macros::M4S_FILE_EXT) || filename.ends_with(macros::TOML_FILE_EXT) ||
         filename.ends_with(macros::OWNER_FILE_EXT);
}

auto validate_m3u8_format(const PlaylistData& content) -> bool
{
  return content.find(macros::PLAYLIST_GLOBAL_HEADER) != std::string::npos;
}

auto validate_ts_file(const AudioBuffer& data) -> bool
{
  return !data.empty() && data[0] == TRANSPORT_STREAM_START_BYTE; // MPEG-TS sync byte
}

// This validation is NOT correct, will change this in future.
WAVY_DEPRECATED("Validating m4s files feature is deprecated and a new one is coming soon!")
auto validate_m4s(const RelPath& m4s_path) -> bool
{
  // std::ifstream file(m4s_path, std::ios::binary);
  // if (!file.is_open())
  // {
  //   LOG_ERROR << SERVER_VALIDATE_LOG << "Failed to open .m4s file: " << m4s_path;
  //   return false;
  // }
  //
  // // Read the first 12 bytes (enough to check for 'ftyp' and a major brand)
  // std::vector<uint8_t> header(12);
  // file.read(reinterpret_cast<char*>(header.data()), header.size());
  //
  // if (file.gcount() < 12)
  // {
  //   LOG_ERROR << SERVER_VALIDATE_LOG << ".m4s file too small: " << m4s_path;
  //   return false;
  // }
  //
  // // First 4 bytes: Box size (big-endian)
  // uint32_t box_size = boost::endian::big_to_native(*reinterpret_cast<uint32_t*>(header.data()));
  //
  // // Next 4 bytes: Box type (should be 'ftyp')
  // std::string box_type(reinterpret_cast<char*>(header.data() + 4), 4);
  //
  // if (box_type != "ftyp")
  // {
  //   LOG_WARNING << SERVER_VALIDATE_LOG << "Missing 'ftyp' header in .m4s: " << m4s_path;
  //   return false;
  // }
  //
  // // Ensure the file contains 'moof' (Movie Fragment) and 'mdat' (Media Data)
  // file.seekg(0, std::ios::beg);
  // std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
  //
  // if (content.find("moof") == std::string::npos || content.find("mdat") == std::string::npos)
  // {
  //   LOG_WARNING << SERVER_VALIDATE_LOG
  //               << "Possible invalid .m4s segment (missing 'moof' or 'mdat'): " << m4s_path;
  //   return false;
  // }

  return true;
}

auto extract_payload(const RelPath& payload_path, const RelPath& extract_path) -> bool
{
  log::INFO<SExtract>("Extracting PAYLOAD: {}", payload_path);

  struct archive*       a   = archive_read_new();
  struct archive*       ext = archive_write_disk_new();
  struct archive_entry* entry;

  archive_read_support_filter_gzip(a);
  archive_read_support_format_tar(a);
  archive_write_disk_set_options(ext, ARCHIVE_EXTRACT_PERM);

  if (archive_read_open_filename(a, payload_path.c_str(), ARCHIVE_READ_BUFFER_SIZE) != ARCHIVE_OK)
  {
    log::ERROR<SExtract>("Failed to open archive: {}", archive_error_string(a));
    archive_read_free(a);
    archive_write_free(ext);
    return false;
  }

  bool valid_files_found = false;

  while (archive_read_next_header(a, &entry) == ARCHIVE_OK)
  {
    RelPath filename    = archive_entry_pathname(entry);
    AbsPath output_file = extract_path + "/" + filename;

    log::TRACE<SExtract>("Extracting file: {}",
                         bfs::relative(output_file, macros::SERVER_STORAGE_DIR).string());

    archive_entry_set_pathname(entry, output_file.c_str());

    if (archive_write_header(ext, entry) == ARCHIVE_OK)
    {
      std::ofstream ofs(output_file, std::ios::binary);
      if (!ofs)
      {
        log::ERROR<SExtract>("Failed to open file for writing: {}", output_file);
        continue;
      }

      char    buffer[8192]; // maybe better off using std::array<>?
      ssize_t len;
      while ((len = archive_read_data(a, buffer, sizeof(buffer))) > 0)
      {
        ofs.write(buffer, len);
      }
      ofs.close();

      valid_files_found = true;

      // If the extracted file is a .zst file, decompress it
      if (output_file.substr(output_file.find_last_of(".") + 1) == macros::ZSTD_FILE_EXT)
      {
        log::TRACE<log::NONE>("[ZSTD] Decompressing .zst file: {}",
                              bfs::relative(output_file, macros::SERVER_TEMP_STORAGE_DIR).string());
        if (!ZSTD_decompress_file(output_file.c_str()))
        {
          log::ERROR<log::NONE>("[ZSTD] Failed to decompress .zst file: {}", output_file);
          continue;
        }

        RelPath decompressed_filename =
          output_file.substr(0, output_file.find_last_of(".")); // remove .zst extension
        log::INFO<SExtract>(
          "Decompressed file: {}",
          bfs::relative(decompressed_filename, macros::SERVER_TEMP_STORAGE_DIR).string());

        if (std::remove(output_file.c_str()) == 0)
        {
          log::TRACE<log::NONE>(
            "[ZSTD] Deleted the original .zst file: {}",
            bfs::relative(output_file, macros::SERVER_TEMP_STORAGE_DIR).string());
        }
        else
        {
          log::ERROR<log::NONE>(
            "[ZSTD] Failed to delete .zst file: {}",
            bfs::relative(output_file, macros::SERVER_TEMP_STORAGE_DIR).string());
        }
      }
    }
  }

  archive_read_free(a);
  archive_write_free(ext);

  return valid_files_found;
}

auto extract_and_validate(const RelPath& gzip_path, const StorageAudioID& audio_id) -> bool
{
  log::INFO<SExtract>(LogMode::Async, " Validating and extracting GZIP file: {}", gzip_path);

  if (!bfs::exists(gzip_path))
  {
    log::ERROR<SExtract>(LogMode::Async, " File does not exist: {}", gzip_path);
    return false;
  }

  const AbsPath temp_extract_path =
    macros::to_string(macros::SERVER_TEMP_STORAGE_DIR) + "/" + audio_id;
  bfs::create_directories(temp_extract_path);

  if (!extract_payload(gzip_path, temp_extract_path))
  {
    log::ERROR<SExtract>(LogMode::Async, " Extraction failed!");
    return false;
  }

  log::INFO<SExtract>(LogMode::Async, " Extraction complete. Scanning for owner file...");

  StorageOwnerID ownerNickname;
  bool           ownerFound = false;

  // First pass: find the owner file
  for (const bfs::directory_entry& file : bfs::directory_iterator(temp_extract_path))
  {
    FileName fname = file.path().filename().string();
    if (fname.ends_with(macros::OWNER_FILE_EXT))
    {
      ownerNickname = fname;
      ownerFound    = true;
      log::INFO<SExtract>(LogMode::Async, " Found OWNER nickname file: {}", fname);
      break;
    }
  }

  if (!ownerFound)
  {
    log::ERROR<SExtract>(LogMode::Async, " Missing OWNER file. Cannot determine destination path.");
    return false;
  }

  const AbsPath storage_path =
    macros::to_string(macros::SERVER_STORAGE_DIR) + "/" + ownerNickname + "/" + audio_id;
  bfs::create_directories(storage_path);

  log::INFO<SExtract>(LogMode::Async, " Validating and moving extracted files...");

  int valid_file_count  = 0;
  int metadataFileCount = 0;

  // Second pass: validate and move files
  for (const bfs::directory_entry& file : bfs::directory_iterator(temp_extract_path))
  {
    FileName fname = file.path().filename().string();

    // Skip the owner file, we already handled it
    if (fname.ends_with(macros::OWNER_FILE_EXT))
      continue;

    std::ifstream infile(file.path().string(), std::ios::binary);
    if (!infile)
    {
      log::WARN<SExtract>(LogMode::Async, " Could not open file: {}", fname);
      bfs::remove(file.path());
      continue;
    }

    std::vector<ui8> data((std::istreambuf_iterator<char>(infile)), {});

    if (fname.ends_with(macros::PLAYLIST_EXT))
    {
      if (!validate_m3u8_format(std::string(data.begin(), data.end())))
      {
        log::WARN<SExtract>(LogMode::Async, " Invalid M3U8 file, removing: {}", fname);
        bfs::remove(file.path());
        continue;
      }
    }
    else if (fname.ends_with(macros::TRANSPORT_STREAM_EXT))
    {
      if (!validate_ts_file(data))
      {
        log::WARN<SExtract>(LogMode::Async, " Invalid TS file, removing: {}", fname);
        bfs::remove(file.path());
        continue;
      }
    }
    else if (fname.ends_with(macros::M4S_FILE_EXT))
    {
      if (!validate_m4s(file.path().string()))
      {
        log::TRACE<SExtract>(LogMode::Async, " Possibly invalid M4S segment: {}", fname);
      }
    }
    else if (fname.ends_with(macros::MP4_FILE_EXT))
    {
      log::DBG<SExtract>(LogMode::Async, " Found MP4 file: {}", fname);
    }
    else if (fname.ends_with(macros::TOML_FILE_EXT))
    {
      if (metadataFileCount++ > 0)
      {
        log::WARN<SExtract>(LogMode::Async, " Extra metadata TOML file ignored: {}", fname);
        bfs::remove(file.path());
        continue;
      }

      log::DBG<SExtract>(LogMode::Async, " Found metadata TOML file: {}", fname);
    }
    else
    {
      log::WARN<SExtract>(" Unknown file, removing: {}", fname);
      bfs::remove(file.path());
      continue;
    }

    // Move to storage
    bfs::rename(file.path(), storage_path + "/" + fname);
    log::INFO<SExtract>(LogMode::Async, " File stored: {}", fname);
    valid_file_count++;
  }

  if (valid_file_count == 0)
  {
    log::ERROR<SExtract>(LogMode::Async,
                         " No valid files remain after validation. Extraction failed.");
    return false;
  }

  log::INFO<SExtract>(LogMode::Async, " Extraction and validation successful.");
  return true;
}

void removeBodyPadding(std::string& body)
{
  auto pos = body.find(macros::NETWORK_TEXT_DELIM);
  if (pos != std::string::npos)
  {
    body = body.substr(pos + macros::NETWORK_TEXT_DELIM.length());
  }

  // Remove bottom padding text
  std::string bottom_delimiter = "--------------------------";
  auto        bottom_pos       = body.find(bottom_delimiter);
  if (bottom_pos != std::string::npos)
  {
    body = body.substr(0, bottom_pos);
  }
}

auto tokenizePath(std::istringstream& iss) -> vector<std::string>
{
  std::string         token;
  vector<std::string> parts;
  while (std::getline(iss, token, '/'))
  {
    if (!token.empty())
    {
      parts.push_back(token);
    }
  }

  return parts;
}

} //namespace libwavy::server::helpers
