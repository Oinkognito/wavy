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

#include <boost/uuid/random_generator.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <crow.h>
#include <libwavy/common/macros.hpp>
#include <libwavy/log-macros.hpp>
#include <libwavy/server/auth.hpp>
#include <libwavy/server/prototypes.hpp>
#include <libwavy/server/request-timer.hpp>
#include <libwavy/toml/toml_parser.hpp>
#include <sstream>

using Server       = libwavy::log::SERVER;
using ServerUpload = libwavy::log::SERVER_UPLD;

namespace libwavy::server::methods
{

class OwnerManager
{
public:
  OwnerManager(Metrics& metrics) : m_metrics(metrics) {}

  auto list_owners() -> crow::response
  {
    RequestTimer timer(m_metrics);

    try
    {
      log::INFO<Server>(LogMode::Async, "Handling Nicknames Listing Request (NLR)");

      AbsPath storage_path = macros::to_string(macros::SERVER_STORAGE_DIR);
      if (!bfs::exists(storage_path) || !bfs::is_directory(storage_path))
      {
        log::ERROR<Server>(LogMode::Async, "Storage directory not found: {}", storage_path);
        timer.mark_error_500();
        return {500, macros::to_string(macros::SERVER_ERROR_500)};
      }

      std::ostringstream response_stream;
      bool               entries_found = false;

      for (const bfs::directory_entry& nickname_entry : bfs::directory_iterator(storage_path))
      {
        if (bfs::is_directory(nickname_entry.status()))
        {
          const StorageOwnerID nickname = nickname_entry.path().filename().string();
          response_stream << nickname << ":\n";

          bool audio_found = false;
          for (const bfs::directory_entry& audio_entry :
               bfs::directory_iterator(nickname_entry.path()))
          {
            if (bfs::is_directory(audio_entry.status()))
            {
              response_stream << "  - " << audio_entry.path().filename().string() << "\n";
              audio_found = true;
            }
          }

          if (!audio_found)
            response_stream << "  (No audio IDs found)\n";

          entries_found = true;
        }
      }

      if (!entries_found)
      {
        log::ERROR<Server>(LogMode::Async, "No IPs or Audio-IDs found in storage!!");
        timer.mark_error_404();
        return {404, macros::to_string(macros::SERVER_ERROR_404)};
      }

      timer.mark_success();
      return {200, response_stream.str()};
    }
    catch (const std::exception& e)
    {
      log::ERROR<Server>(LogMode::Async, "Exception in owners endpoint: {}", e.what());
      timer.mark_error_500();
      return {500, "Internal Server Error"};
    }
  }

  auto list_audio_info() -> crow::response
  {
    RequestTimer timer(m_metrics);

    try
    {
      log::INFO<Server>(LogMode::Async, "Handling Audio Metadata Listing request (AMLR)");

      AbsPath storage_path = macros::to_string(macros::SERVER_STORAGE_DIR);
      if (!bfs::exists(storage_path) || !bfs::is_directory(storage_path))
      {
        log::ERROR<Server>(LogMode::Async, "Storage directory not found: {}", storage_path);
        timer.mark_error_500();
        return {500, macros::to_string(macros::SERVER_ERROR_500)};
      }

      std::ostringstream response_stream;
      bool               entries_found = false;

      auto fetch_metadata = [&](const AbsPath&        metadata_path,
                                const StorageAudioID& audio_id) -> bool
      {
        try
        {
          AudioMetadata metadata = parseAudioMetadata(metadata_path);
          response_stream << "  - " << audio_id << "\n";
          response_stream << "      1. Title: " << metadata.title << "\n";
          response_stream << "      2. Artist: " << metadata.artist << "\n";
          response_stream << "      3. Duration: " << metadata.duration << " secs\n";
          response_stream << "      4. Album: " << metadata.album << "\n";
          response_stream << "      5. Bitrate: " << metadata.bitrate << " kbps\n";
          response_stream << "      6. Sample Rate: " << metadata.audio_stream.sample_rate
                          << " Hz\n";
          response_stream << "      7. Sample Format: " << metadata.audio_stream.sample_format
                          << "\n";
          response_stream << "      8. Audio Bitrate: " << metadata.audio_stream.bitrate
                          << " kbps\n";
          response_stream << "      9. Codec: " << metadata.audio_stream.codec << "\n";
          response_stream << "      10. Available Bitrates: [";
          for (auto br : metadata.bitrates)
            response_stream << br << ",";
          response_stream << "]\n";
        }
        catch (const std::exception& e)
        {
          log::ERROR<Server>(LogMode::Async, "Error parsing metadata for Audio-ID {}: {}", audio_id,
                             e.what());
          return false;
        }
        return true;
      };

      for (const bfs::directory_entry& nickname_entry : bfs::directory_iterator(storage_path))
      {
        if (bfs::is_directory(nickname_entry.status()))
        {
          const StorageOwnerID nickname = nickname_entry.path().filename().string();
          response_stream << nickname << ":\n";

          bool audio_found = false;
          for (const bfs::directory_entry& audio_entry :
               bfs::directory_iterator(nickname_entry.path()))
          {
            if (bfs::is_directory(audio_entry.status()))
            {
              RelPath metadata_path =
                audio_entry.path().string() + "/" + macros::to_cstr(macros::METADATA_FILE);
              if (bfs::exists(metadata_path))
              {
                if (fetch_metadata(metadata_path, audio_entry.path().filename().string()))
                  audio_found = true;
              }
            }
          }

          if (!audio_found)
            response_stream << "  (No metadata found)\n";

          entries_found = true;
        }
      }

      if (!entries_found)
      {
        timer.mark_error_404();
        return {404, macros::to_string(macros::SERVER_ERROR_404)};
      }

      timer.mark_success();
      return {200, response_stream.str()};
    }
    catch (const std::exception& e)
    {
      log::ERROR<Server>(LogMode::Async, "Exception in audio info endpoint: {}", e.what());
      timer.mark_error_500();
      return {500, "Internal Server Error"};
    }
  }

  auto handle_upload(const crow::request& req) -> crow::response
  {
    RequestTimer timer(m_metrics);

    try
    {
      log::INFO<ServerUpload>(LogMode::Async, "Handling GZIP file upload");

      // Validate request size
      if (req.body.empty())
      {
        log::ERROR<ServerUpload>(LogMode::Async, "Upload request with empty body");
        timer.mark_error_400();
        return {400, "Empty upload request"};
      }

      if (req.body.size() > WAVY_SERVER_UPLOAD_SIZE_LIMIT * 1024 * 1024)
      { // 100MB limit
        log::ERROR<ServerUpload>(LogMode::Async, "Upload too large: {} bytes", req.body.size());
        timer.mark_error_400();
        return {413, "Upload too large"};
      }

      const StorageAudioID audio_id  = boost::uuids::to_string(boost::uuids::random_generator()());
      const AbsPath        gzip_path = macros::to_string(macros::SERVER_TEMP_STORAGE_DIR) + "/" +
                                audio_id + macros::to_string(macros::COMPRESSED_ARCHIVE_EXT);

      boost::filesystem::create_directories(macros::SERVER_TEMP_STORAGE_DIR);

      std::ofstream output_file(gzip_path, std::ios::binary);
      if (!output_file)
      {
        log::ERROR<ServerUpload>(LogMode::Async, "Failed to create temp file: {}", gzip_path);
        timer.mark_error_500();
        return {500, "Failed to create temporary file"};
      }

      output_file.write(req.body.data(), req.body.size());
      output_file.close();

      if (!boost::filesystem::exists(gzip_path) || boost::filesystem::file_size(gzip_path) == 0)
      {
        log::ERROR<ServerUpload>(LogMode::Async, "GZIP upload failed: File is empty or missing!");
        timer.mark_error_400();
        return {400, "GZIP upload failed"};
      }

      StorageOwnerID ownerNickname = helpers::extract_and_validate(gzip_path, audio_id);
      m_metrics.record_owner_upload(ownerNickname, req.body.size());

      if (!ownerNickname.empty())
      {
        log::INFO<ServerUpload>("Computing HASH for Owner: {}", ownerNickname);

        auto sha_opt = auth::compute_sha256_hex(gzip_path);
        if (!sha_opt)
        {
          log::WARN<ServerUpload>(LogMode::Async, "Failed to compute SHA-256 for Audio-ID: {}",
                                  audio_id);
        }
        bool key_persisted = false;
        if (sha_opt)
          key_persisted = auth::persist_key(audio_id, *sha_opt);

        // Clean up temporary archive
        boost::filesystem::remove(gzip_path);

        timer.mark_success();
        log::INFO<ServerUpload>(LogMode::Async, "Upload successful, Audio-ID: {}", audio_id);

        std::ostringstream body;
        body << "audio_id=" << audio_id << "\n";

        if (sha_opt)
        {
          body << "sha256=" << *sha_opt << "\n";
          body << "key_persisted=" << (key_persisted ? "true" : "false") << "\n";
        }
        else
        {
          body << "sha256=\n";
          body << "key_persisted=false\n";
        }

        crow::response res;
        res.code = 200;
        res.set_header("Content-Type", "text/plain");
        res.body = body.str();
        return res;
      }
      else
      {
        boost::filesystem::remove(gzip_path);
        timer.mark_error_400();
        return {400, macros::to_string(macros::SERVER_ERROR_400)};
      }
    }
    catch (const std::exception& e)
    {
      log::ERROR<ServerUpload>(LogMode::Async, "Exception in upload endpoint: {}", e.what());
      timer.mark_error_500();
      return {500, "Internal Server Error"};
    }
  }

  auto handle_delete(const crow::request& req, const StorageOwnerID& ownerID,
                     const StorageAudioID& audio_id) -> crow::response
  {
    RequestTimer timer(m_metrics);
    m_metrics.record_owner_delete(ownerID);

    try
    {
      // Extract provided SHA256 key from query or header
      std::string provided_key;
      if (auto it = req.url_params.get("sha256"); it)
      {
        provided_key = it;
      }
      else
      {
        log::ERROR<Server>(LogMode::Async, "Missing sha256 parameter");
        timer.mark_error_400();
        return {400, "Missing 'sha256' parameter"};
      }

      namespace bfs      = boost::filesystem;
      bfs::path keys_dir = bfs::path(macros::to_string(macros::SERVER_STORAGE_DIR)) / ".keys";
      bfs::path key_file = keys_dir / (audio_id + ".key");

      if (!bfs::exists(key_file))
      {
        log::ERROR<Server>(LogMode::Async, "No key file for Audio-ID: {}", audio_id);
        timer.mark_error_404();
        return {404, "Audio-ID not found"};
      }

      // Read stored key
      std::ifstream ifs(key_file, std::ios::binary);
      std::string   stored_key;
      std::getline(ifs, stored_key);
      ifs.close();

      if (stored_key != provided_key)
      {
        log::WARN<Server>(LogMode::Async, "SHA256 key mismatch for Audio-ID: {}", audio_id);
        timer.mark_error_403();
        return {403, "Invalid key"};
      }

      // Delete the audio directory
      bfs::path audio_dir =
        bfs::path(macros::to_string(macros::SERVER_STORAGE_DIR)) / ownerID / audio_id;
      if (bfs::exists(audio_dir) && bfs::is_directory(audio_dir))
      {
        bfs::remove_all(audio_dir);
      }

      // Remove the key file
      bfs::remove(key_file);

      timer.mark_success();
      log::INFO<Server>(LogMode::Async, "Successfully deleted Audio-ID: {}", audio_id);

      crow::response res;
      res.code = 200;
      res.set_header("Content-Type", "text/plain");
      res.body = "Deleted Audio-ID: " + audio_id + "\n";
      return res;
    }
    catch (const std::exception& e)
    {
      log::ERROR<Server>(LogMode::Async, "Exception in delete endpoint: {}", e.what());
      timer.mark_error_500();
      return {500, "Internal Server Error"};
    }
  }

private:
  Metrics& m_metrics;
};

} // namespace libwavy::server::methods
