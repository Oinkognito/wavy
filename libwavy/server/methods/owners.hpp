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
  OwnerManager(Metrics& metrics, OwnerAudioIDMap& g_owner_audio_db)
      : m_metrics(metrics), m_owner_audio_db(g_owner_audio_db)
  {
  }

  auto list_owners() -> crow::response
  {
    RequestTimer req_timer(m_metrics);

    try
    {
      log::INFO<Server>(LogMode::Async, "Handling Nicknames Listing Request (NLR)");

      std::ostringstream response_stream;
      bool               entries_found = false;

      // Traverse DB owner by owner
      m_owner_audio_db.for_each_owner(
        [&](const StorageOwnerID&                                    owner,
            const MiniDB<StorageOwnerID, StorageAudioID>::audio_set& audios)
        {
          response_stream << owner << ":\n";
          if (audios.empty())
          {
            response_stream << "  (No audio IDs found)\n";
          }
          else
          {
            for (const auto& audio_id : audios)
              response_stream << "  - " << audio_id << "\n";
          }
          entries_found = true;
        });

      if (!entries_found)
      {
        log::ERROR<Server>(LogMode::Async, "No Owners or Audio-IDs in DB!!");
        req_timer.mark_error_404();
        return {404, macros::to_string(macros::SERVER_ERROR_404)};
      }

      req_timer.mark_success();
      return {200, response_stream.str()};
    }
    catch (const std::exception& e)
    {
      log::ERROR<Server>(LogMode::Async, "Exception in owners endpoint: {}", e.what());
      req_timer.mark_error_500();
      return {500, "Internal Server Error"};
    }
  }

  auto list_audio_info() -> crow::response
  {
    RequestTimer req_timer(m_metrics);

    try
    {
      log::INFO<Server>(LogMode::Async, "Handling Audio Metadata Listing request (AMLR)");

      std::ostringstream response_stream;
      bool               entries_found = false;

      m_owner_audio_db.for_each_owner(
        [&](const StorageOwnerID& owner, const auto& audio_ids)
        {
          response_stream << owner << ":\n";

          for (const auto& audio_id : audio_ids)
          {
            // build metadata path from storage directory
            AbsPath metadata_path =
              AbsPath(macros::SERVER_STORAGE_DIR) / owner / audio_id / macros::METADATA_FILE;

            if (fs::exists(metadata_path))
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

                entries_found = true;
              }
              catch (const std::exception& e)
              {
                log::ERROR<Server>(LogMode::Async, "Error parsing metadata for Audio-ID {}: {}",
                                   audio_id, e.what());
              }
            }
          }
        });

      if (!entries_found)
      {
        req_timer.mark_error_404();
        return {404, macros::to_string(macros::SERVER_ERROR_404)};
      }

      req_timer.mark_success();
      return {200, response_stream.str()};
    }
    catch (const std::exception& e)
    {
      log::ERROR<Server>(LogMode::Async, "Exception in audio info endpoint: {}", e.what());
      req_timer.mark_error_500();
      return {500, "Internal Server Error"};
    }
  }

  auto handle_upload(const crow::request& req) -> crow::response
  {
    RequestTimer req_timer(m_metrics);

    try
    {
      log::INFO<ServerUpload>(LogMode::Async, "Handling GZIP file upload");

      // Validate request size
      if (req.body.empty())
      {
        log::ERROR<ServerUpload>(LogMode::Async, "Upload request with empty body");
        req_timer.mark_error_400();
        return {400, "Empty upload request"};
      }

      if (req.body.size() > WAVY_SERVER_UPLOAD_SIZE_LIMIT * 1024 * 1024)
      { // 100MB limit
        log::ERROR<ServerUpload>(LogMode::Async, "Upload too large: {} bytes", req.body.size());
        req_timer.mark_error_400();
        return {413, "Upload too large"};
      }

      const StorageAudioID audio_id = boost::uuids::to_string(boost::uuids::random_generator()());
      const AbsPath        gzip_path =
        AbsPath(macros::SERVER_TEMP_STORAGE_DIR) / audio_id + macros::COMPRESSED_ARCHIVE_EXT;

      fs::create_directories(macros::SERVER_TEMP_STORAGE_DIR);

      std::ofstream output_file(gzip_path, std::ios::binary);
      if (!output_file)
      {
        log::ERROR<ServerUpload>(LogMode::Async, "Failed to create temp file: {}", gzip_path.str());
        req_timer.mark_error_500();
        return {500, "Failed to create temporary file"};
      }

      output_file.write(req.body.data(), req.body.size());
      output_file.close();

      if (!fs::exists(gzip_path) || fs::file_size(gzip_path) == 0)
      {
        log::ERROR<ServerUpload>(LogMode::Async, "GZIP upload failed: File is empty or missing!");
        req_timer.mark_error_400();
        return {400, "GZIP upload failed"};
      }

      StorageOwnerID ownerNickname =
        helpers::extract_and_validate(gzip_path, audio_id, m_owner_audio_db);
      m_metrics.record_owner_upload(ownerNickname, req.body.size());

      if (!ownerNickname.empty())
      {
        log::TRACE<ServerUpload>("Computing HASH for Owner: {}", ownerNickname);

        auto sha_opt = auth::compute_sha256_hex(gzip_path);
        if (!sha_opt)
        {
          log::ERROR<ServerUpload>(LogMode::Async, "Failed to compute SHA-256 for Audio-ID: {}",
                                   audio_id);
        }
        bool key_persisted = false;
        if (sha_opt)
          key_persisted = auth::persist_key(audio_id, *sha_opt);

        // Clean up temporary archive
        fs::remove(gzip_path);

        req_timer.mark_success();
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
        fs::remove(gzip_path);
        req_timer.mark_error_400();
        return {400, macros::to_string(macros::SERVER_ERROR_400)};
      }
    }
    catch (const std::exception& e)
    {
      log::ERROR<ServerUpload>(LogMode::Async, "Exception in upload endpoint: {}", e.what());
      req_timer.mark_error_500();
      return {500, "Internal Server Error"};
    }
  }

  auto handle_delete(const crow::request& req, const StorageOwnerID& ownerID,
                     const StorageAudioID& audio_id) -> crow::response
  {
    RequestTimer req_timer(m_metrics);
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
        req_timer.mark_error_400();
        return {400, "Missing 'sha256' parameter"};
      }

      fs::path keys_dir = fs::path(macros::to_string(macros::SERVER_STORAGE_DIR)) / ".keys";
      fs::path key_file = keys_dir / (audio_id + ".key");

      if (!fs::exists(key_file))
      {
        log::ERROR<Server>(LogMode::Async, "No key file for Audio-ID: {}", audio_id);
        req_timer.mark_error_404();
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
        req_timer.mark_error_403();
        return {403, "Invalid key"};
      }

      // Delete the audio directory
      const AbsPath audio_dir = AbsPath(macros::SERVER_STORAGE_DIR) / ownerID / audio_id;
      if (fs::exists(audio_dir) && fs::is_directory(audio_dir))
      {
        fs::remove_all(audio_dir);
      }

      // Remove the key file
      fs::remove(key_file);

      req_timer.mark_success();
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
      req_timer.mark_error_500();
      return {500, "Internal Server Error"};
    }
  }

private:
  Metrics&         m_metrics;
  OwnerAudioIDMap& m_owner_audio_db;
};

} // namespace libwavy::server::methods
