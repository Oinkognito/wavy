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
#include <libwavy/common/network/routes.h>
#include <libwavy/db/entry.hpp>
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
  OwnerManager(Metrics& metrics, db::LMDBKV<AudioMetadataPlain>& kv)
      : m_metrics(metrics), m_kvStore(kv)
  {
  }

  // Returns true if owner_map was updated
  auto populateOwnerMap() -> bool
  {
    owner_map.clear();

    m_kvStore.for_(
      [&](const db::Key& key, const db::Value& val)
      {
        // key format: "<nickname>/<audio_id>/filename"
        log::TRACE<Server>("Key: {}", key);
        auto first_sep = key.find('/');
        if (first_sep == std::string::npos)
          return;

        StorageOwnerID nickname = key.substr(0, first_sep);

        auto second_sep = key.find('/', first_sep + 1);
        if (second_sep == std::string::npos)
          return;

        StorageAudioID audio_id = key.substr(first_sep + 1, second_sep - first_sep - 1);

        owner_map[nickname].insert(audio_id);
      });

    return true; // cache updated
  }

  auto list_owners() -> crow::response
  {
    RequestTimer timer(m_metrics);

    try
    {
      log::INFO<Server>(LogMode::Async, "Handling Nicknames Listing Request (NLR)");

      std::ostringstream response_stream;
      bool               entries_found = false;

      if (!populateOwnerMap())
      {
        log::TRACE<Server>("Owner map cache is up-to-date, skipping LMDB iteration.");
      }

      if (owner_map.empty())
      {
        log::ERROR<Server>(LogMode::Async, "No owners or Audio-IDs found in LMDB storage!!");
        timer.mark_error_404();
        return {404, macros::to_string(macros::SERVER_ERROR_404)};
      }

      // iterate owner_map_cache as before
      for (const auto& [nickname, audio_ids] : owner_map)
      {
        response_stream << nickname << ":\n";
        if (audio_ids.empty())
          response_stream << "  (No audio IDs found)\n";
        else
          for (const auto& audio_id : audio_ids)
            response_stream << "  - " << audio_id << "\n";
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

      std::ostringstream response_stream;
      bool               entries_found = false;

      populateOwnerMap(); // updates owner_map_cache if needed

      if (owner_map.empty())
      {
        log::WARN<Server>(LogMode::Async, "Owner Map found to be empty...");
        timer.mark_error_404();
        return {404, macros::to_string(macros::SERVER_ERROR_404)};
      }

      // iterate owner_map_cache for metadata
      for (const auto& [nickname, audio_ids] : owner_map)
      {
        response_stream << nickname << ":\n";
        for (const auto& audio_id : audio_ids)
        {
          const db::Key metadata_key =
            db::make_kv_key(nickname, audio_id, macros::to_string(macros::METADATA_FILE));

          auto plain = m_kvStore.get(metadata_key);
          if (plain.empty())
          {
            log::WARN<Server>(LogMode::Async, "Metadata missing for key in store: '{}'",
                              metadata_key);
            continue;
          }

          auto metadata = parseAudioMetadataFromDataString(as::str(plain));

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
        entries_found = true;
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
      log::ERROR<Server>(LogMode::Async, "Exception in {} endpoint: {}",
                         routes::SERVER_PATH_AUDIO_INFO, e.what());
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

      StorageOwnerID ownerNickname = helpers::extract_and_validate(gzip_path, audio_id, m_kvStore);
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
        log::ERROR<Server>("Missing sha256 parameter");
        timer.mark_error_400();
        return {400, "Missing 'sha256' parameter"};
      }

      namespace bfs      = boost::filesystem;
      bfs::path keys_dir = bfs::path(macros::to_string(macros::SERVER_STORAGE_DIR)) / ".keys";
      bfs::path key_file = keys_dir / (audio_id + ".key");

      if (!bfs::exists(key_file))
      {
        log::ERROR<Server>("No key file for Audio-ID: {}", audio_id);
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
        log::WARN<Server>("SHA256 key mismatch for Audio-ID: {}", audio_id);
        timer.mark_error_403();
        return {403, "Invalid key"};
      }

      // Delete all KV entries under <owner-id>/<audio-id> prefix
      const std::string prefix = ownerID + "/" + audio_id + "/";
      m_kvStore.for_(
        [&](const db::Key& key, const db::Value&)
        {
          if (key.starts_with(prefix))
          {
            m_kvStore.erase(key);
            log::DBG<Server>("Deleted key from KV store: {}", key);
          }
        });

      timer.mark_success();
      log::INFO<Server>("Successfully deleted Audio-ID: {} for owner {}", audio_id, ownerID);

      crow::response res;
      res.code = 200;
      res.set_header("Content-Type", "text/plain");
      res.body = "Deleted Audio-ID: " + audio_id + "\n";
      return res;
    }
    catch (const std::exception& e)
    {
      log::ERROR<Server>("Exception in delete endpoint: {}", e.what());
      timer.mark_error_500();
      return {500, "Internal Server Error"};
    }
  }

private:
  Metrics&                                                                   m_metrics;
  db::LMDBKV<AudioMetadataPlain>&                                            m_kvStore;
  inline static std::unordered_map<StorageOwnerID, std::set<StorageAudioID>> owner_map;
};

} // namespace libwavy::server::methods
