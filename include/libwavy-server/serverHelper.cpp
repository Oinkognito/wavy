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

#include "server.hpp"

auto is_valid_extension(const std::string& filename) -> bool
{
  return filename.ends_with(macros::PLAYLIST_EXT) ||
         filename.ends_with(macros::TRANSPORT_STREAM_EXT) ||
         filename.ends_with(macros::M4S_FILE_EXT) || filename.ends_with(macros::TOML_FILE_EXT);
}

auto validate_m3u8_format(const std::string& content) -> bool
{
  return content.find(macros::PLAYLIST_GLOBAL_HEADER) != std::string::npos;
}

auto validate_ts_file(const std::vector<uint8_t>& data) -> bool
{
  return !data.empty() && data[0] == TRANSPORT_STREAM_START_BYTE; // MPEG-TS sync byte
}

auto validate_m4s(const std::string& m4s_path) -> bool
{
  std::ifstream file(m4s_path, std::ios::binary);
  if (!file.is_open())
  {
    LOG_ERROR << SERVER_VALIDATE_LOG << "Failed to open .m4s file: " << m4s_path;
    return false;
  }

  // Read the first 12 bytes (enough to check for 'ftyp' and a major brand)
  std::vector<uint8_t> header(12);
  file.read(reinterpret_cast<char*>(header.data()), header.size());

  if (file.gcount() < 12)
  {
    LOG_ERROR << SERVER_VALIDATE_LOG << ".m4s file too small: " << m4s_path;
    return false;
  }

  // First 4 bytes: Box size (big-endian)
  uint32_t box_size = boost::endian::big_to_native(*reinterpret_cast<uint32_t*>(header.data()));

  // Next 4 bytes: Box type (should be 'ftyp')
  std::string box_type(reinterpret_cast<char*>(header.data() + 4), 4);

  if (box_type != "ftyp")
  {
    LOG_ERROR << SERVER_VALIDATE_LOG << "Missing 'ftyp' header in .m4s: " << m4s_path;
    return false;
  }

  // Ensure the file contains 'moof' (Movie Fragment) and 'mdat' (Media Data)
  file.seekg(0, std::ios::beg);
  std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

  if (content.find("moof") == std::string::npos || content.find("mdat") == std::string::npos)
  {
    LOG_ERROR << SERVER_VALIDATE_LOG
              << "Invalid .m4s segment (missing 'moof' or 'mdat'): " << m4s_path;
    return false;
  }

  LOG_INFO << SERVER_VALIDATE_LOG << "Valid .m4s segment: " << m4s_path;
  return true;
}

auto extract_payload(const std::string& payload_path, const std::string& extract_path) -> bool
{
  LOG_INFO << SERVER_EXTRACT_LOG << "Extracting PAYLOAD: " << payload_path;

  struct archive*       a   = archive_read_new();
  struct archive*       ext = archive_write_disk_new();
  struct archive_entry* entry;

  archive_read_support_filter_gzip(a);
  archive_read_support_format_tar(a);
  archive_write_disk_set_options(ext, ARCHIVE_EXTRACT_PERM);

  if (archive_read_open_filename(a, payload_path.c_str(), 10240) != ARCHIVE_OK)
  {
    LOG_ERROR << SERVER_EXTRACT_LOG << "Failed to open archive: " << archive_error_string(a);
    archive_read_free(a);
    archive_write_free(ext);
    return false;
  }

  bool valid_files_found = false;

  while (archive_read_next_header(a, &entry) == ARCHIVE_OK)
  {
    std::string filename    = archive_entry_pathname(entry);
    std::string output_file = extract_path + "/" + filename;

    LOG_INFO << SERVER_EXTRACT_LOG
             << "Extracting file: " << bfs::relative(output_file, macros::SERVER_STORAGE_DIR);

    archive_entry_set_pathname(entry, output_file.c_str());

    if (archive_write_header(ext, entry) == ARCHIVE_OK)
    {
      std::ofstream ofs(output_file, std::ios::binary);
      if (!ofs)
      {
        LOG_ERROR << SERVER_EXTRACT_LOG << "Failed to open file for writing: " << output_file;
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
        LOG_INFO << "[ZSTD] Decompressing .zst file: "
                 << bfs::relative(output_file, macros::SERVER_TEMP_STORAGE_DIR);
        if (!ZSTD_decompress_file(output_file.c_str()))
        {
          LOG_ERROR << "[ZSTD] Failed to decompress .zst file: " << output_file;
          continue;
        }

        std::string decompressed_filename =
          output_file.substr(0, output_file.find_last_of(".")); // remove .zst extension
        LOG_INFO << SERVER_EXTRACT_LOG << "Decompressed file: "
                 << bfs::relative(decompressed_filename, macros::SERVER_TEMP_STORAGE_DIR);

        if (std::remove(output_file.c_str()) == 0)
        {
          LOG_INFO << "[ZSTD] Deleted the original .zst file: "
                   << bfs::relative(output_file, macros::SERVER_TEMP_STORAGE_DIR);
        }
        else
        {
          LOG_ERROR << "[ZSTD] Failed to delete .zst file: "
                    << bfs::relative(output_file, macros::SERVER_TEMP_STORAGE_DIR);
        }
      }
    }
  }

  archive_read_free(a);
  archive_write_free(ext);

  return valid_files_found;
}

auto extract_and_validate(const std::string& gzip_path, const std::string& audio_id,
                          const std::string& ip_id) -> bool
{
  LOG_INFO << SERVER_EXTRACT_LOG << "Validating and extracting GZIP file: " << gzip_path;
  int metadataFileCount = 0;

  if (!bfs::exists(gzip_path))
  {
    LOG_ERROR << SERVER_EXTRACT_LOG << "File does not exist: " << gzip_path;
    return false;
  }

  std::string temp_extract_path =
    macros::to_string(macros::SERVER_TEMP_STORAGE_DIR) + "/" + audio_id;
  bfs::create_directories(temp_extract_path);

  if (!extract_payload(gzip_path, temp_extract_path))
  {
    LOG_ERROR << SERVER_EXTRACT_LOG << "Extraction failed!";
    return false;
  }

  LOG_INFO << SERVER_EXTRACT_LOG << "Extraction complete, validating files...";

  // Move valid files to storage
  std::string storage_path =
    macros::to_string(macros::SERVER_STORAGE_DIR) + "/" + ip_id + "/" + audio_id;
  bfs::create_directories(storage_path);

  int valid_file_count = 0;

  for (const bfs::directory_entry& file : bfs::directory_iterator(temp_extract_path))
  {
    std::string          fname = file.path().filename().string();
    std::ifstream        infile(file.path().string(), std::ios::binary);
    std::vector<uint8_t> data((std::istreambuf_iterator<char>(infile)), {});

    if (fname.ends_with(macros::PLAYLIST_EXT))
    {
      if (!validate_m3u8_format(std::string(data.begin(), data.end())))
      {
        LOG_WARNING << SERVER_EXTRACT_LOG << "Invalid M3U8 file, removing: " << fname;
        bfs::remove(file.path());
        continue;
      }
    }
    else if (fname.ends_with(macros::TRANSPORT_STREAM_EXT))
    {
      if (!validate_ts_file(data))
      {
        LOG_WARNING << SERVER_EXTRACT_LOG << "Invalid TS file, removing: " << fname;
        bfs::remove(file.path());
        continue;
      }
    }
    else if (fname.ends_with(macros::M4S_FILE_EXT))
    {
      if (!validate_m4s(file.path().string())) // Validate .m4s
      {
        LOG_WARNING << SERVER_EXTRACT_LOG << "Possibly invalid M4S segment: " << fname;
      }
    }
    else if (fname.ends_with(macros::MP4_FILE_EXT))
    {
      LOG_DEBUG << SERVER_EXTRACT_LOG << "Found MP4 file: " << fname;
    }
    else if (fname.ends_with(macros::TOML_FILE_EXT) && metadataFileCount == 0)
    {
      LOG_DEBUG << SERVER_EXTRACT_LOG << "Found metadata TOML file: " << fname;
      metadataFileCount++;
    }
    else
    {
      LOG_WARNING << SERVER_EXTRACT_LOG << "Skipping unknown file: " << fname;
      bfs::remove(file.path());
      continue;
    }

    // Move validated file to storage
    bfs::rename(file.path(), storage_path + "/" + fname);
    LOG_INFO << SERVER_EXTRACT_LOG << "File stored in HLS storage: " << fname;
    valid_file_count++;
  }

  if (valid_file_count == 0)
  {
    LOG_ERROR << SERVER_EXTRACT_LOG << "No valid files remain after validation, extraction failed!";
    return false;
  }

  LOG_INFO << SERVER_EXTRACT_LOG << "Extraction and validation successful.";
  return true;
}
