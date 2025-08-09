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

#include "libwavy/common/network/routes.h"
#include <archive.h>
#include <archive_entry.h>
#include <boost/filesystem/directory.hpp>
#include <boost/uuid/random_generator.hpp>
#include <boost/uuid/uuid_io.hpp>

#define CROW_ENABLE_SSL
#include <crow.h>

#include <libwavy/common/api/entry.hpp>
#include <libwavy/common/macros.hpp>
#include <libwavy/common/state.hpp>
#include <libwavy/common/types.hpp>
#include <libwavy/server/methods/download.hpp>
#include <libwavy/server/prototypes.hpp>
#include <libwavy/toml/toml_parser.hpp>
#include <libwavy/unix/domainBind.hpp>
#include <libwavy/zstd/decompression.h>
#include <unistd.h>
#include <utility>

/*
 * @SERVER
 *
 * The server's main responsibility the way we see it is:
 *
 * -> Accepting requests through secure SSL transfer with valid certification (TBD) to upload files
 * (POST)
 * -> Upload of GZip file (which supposedly contains .m3u8 and .ts files with apt references and
 * hierarchial format as expected of HLS encoding)
 * -> Validation of the above GZip file's contents to match expected encoded files
 * -> Assign client a UUID upon validation
 * -> Create and expose the client's uploaded content through UUID
 * -> Serve required files to receiver through GET
 *
 *  Server HLS Storage Organization:
 *
 *  wavy_storage/
 *  ├── <nickname>/                                      # AUDIO OWNER provided nickname
 *  │   ├── 1435f431-a69a-4027-8661-44c31cd11ef6/        # Randomly generated audio id
 *  │   │   ├── index.m3u8
 *  │   │   ├── hls_mp3_64.m3u8                          # HLS MP3 encoded playlist (64-bit)
 *  │   │   ├── hls_mp3_64_0.ts                          # First transport stream of hls_mp3_64 playlist
 *  │   │   ├── ...                                      # Similarly for 128 and 256 bitrates
 *  │   │   ├── metadata.toml                            # Metadata and other song information
 *  │   ├── e5fdeca5-57c8-47b4-b9c6-60492ddf11ae/
 *  │   │   ├── index.m3u8
 *  │   │   ├── hls_flac_64.m3u8                         # HLS FLAC encoded playlist (64-bit)
 *  │   │   ├── hls_flac_64_0.ts                         # First transport stream of hls_mp3_64 playlist
 *  │   │   ├── ...                                      # Similarly for 128 and 256 bitrates
 *  │   │   ├── metadata.toml                            # Metadata and other song information
 *  │
 *
 * Boost libs ensures that every operation (if not then most) in the server occurs asynchronously
 * without any concurrency issues
 *
 * Although not tried and tested with multiple clients, it should give expected results.
 *
 * Boost should also ensure safety and shared lifetimes of a lot of critical objects.
 *
 * @NOTE:
 *
 * ==> This file **CANNOT** be compiled for < -std=c++20
 *
 * Look at Makefile for more information
 *
 * ==> macros::SERVER_STORAGE_DIR and macros::SERVER_TEMP_STORAGE_DIR) are in the same parent
 * directory as boost has problems renaming content to different directories (gives a very big
 * cross-link error)
 *
 */

#define ARCHIVE_READ_BUFFER_SIZE 10240

namespace bfs = boost::filesystem; // Refers to boost filesystem (async file I/O ops)

using Server       = libwavy::log::SERVER;
using ServerUpload = libwavy::log::SERVER_UPLD;

namespace libwavy::server
{

class WAVY_API WavyServer
{
public:
  WavyServer(short port, AbsPath serverCert, AbsPath serverKey)
      : m_socketPath(macros::to_string(macros::SERVER_LOCK_FILE)), m_wavySocketBind(m_socketPath),
        m_port(port), m_serverCert(std::move(serverCert)), m_serverKey(std::move(serverKey))
  {
    m_wavySocketBind.EnsureSingleInstance();
    log::INFO<Server>("Starting Wavy Server on port {}", port);

    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
    std::signal(SIGHUP, signal_handler);
  }

  ~WavyServer() { m_wavySocketBind.cleanup(); }

  void run()
  {
    // Initialize Crow app
    // SSL version — you'll need to configure cert and key here
    crow::App app;

    setup_routes(app);

    app.port(m_port).ssl_file(m_serverCert, m_serverKey).multithreaded().run();
  }

private:
  FileName             m_socketPath;
  unix::UnixSocketBind m_wavySocketBind;
  short                m_port;
  AbsPath              m_serverCert, m_serverKey;

  static void signal_handler(int signo)
  {
    log::INFO<Server>("Termination signal ({}) received. Cleaning up...", signo);
    std::exit(0);
  }

  template <typename CrowApp> void setup_routes(CrowApp& app)
  {
    CROW_ROUTE(app, routes::SERVER_PATH_PING)
      .methods(crow::HTTPMethod::GET)(
        []()
        {
          log::INFO<Server>("Sending pong to client...");
          return crow::response(200, macros::to_string(macros::SERVER_PONG_MSG));
        });

    CROW_ROUTE(app, "/owners")
      .methods(crow::HTTPMethod::GET)(
        []()
        {
          log::INFO<Server>(LogMode::Async, "Handling Nicknames Listing Request (NLR)");

          AbsPath storage_path = macros::to_string(macros::SERVER_STORAGE_DIR);
          if (!bfs::exists(storage_path) || !bfs::is_directory(storage_path))
          {
            log::ERROR<Server>(LogMode::Async, "Storage directory not found: {}", storage_path);
            return crow::response(500, macros::to_string(macros::SERVER_ERROR_500));
          }

          std::ostringstream response_stream;
          bool               entries_found = false;

          for (const bfs::directory_entry& nickname_entry : bfs::directory_iterator(storage_path))
          {
            if (bfs::is_directory(nickname_entry.status()))
            {
              StorageOwnerID nickname = nickname_entry.path().filename().string();
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
            return crow::response(404, macros::to_string(macros::SERVER_ERROR_404));
          }

          return crow::response(200, response_stream.str());
        });

    // Audio Metadata Listing (GET /audio/info)
    CROW_ROUTE(app, routes::SERVER_PATH_AUDIO_INFO)
      .methods(crow::HTTPMethod::GET)(
        []()
        {
          log::INFO<Server>(LogMode::Async, "Handling Audio Metadata Listing request (AMLR)");

          AbsPath storage_path = macros::to_string(macros::SERVER_STORAGE_DIR);
          if (!bfs::exists(storage_path) || !bfs::is_directory(storage_path))
          {
            log::ERROR<Server>(LogMode::Async, "Storage directory not found: {}", storage_path);
            return crow::response(500, macros::to_string(macros::SERVER_ERROR_500));
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
              log::ERROR<Server>(LogMode::Async, "Error parsing metadata for Audio-ID {}: {}",
                                 audio_id, e.what());
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
            return crow::response(404, macros::to_string(macros::SERVER_ERROR_404));

          return crow::response(200, response_stream.str());
        });

    // File upload (POST /upload)
    CROW_ROUTE(app, routes::SERVER_PATH_TOML_UPLOAD)
      .methods(crow::HTTPMethod::POST)(
        [](const crow::request& req)
        {
          log::INFO<ServerUpload>(LogMode::Async, "Handling GZIP file upload");

          const StorageAudioID audio_id =
            boost::uuids::to_string(boost::uuids::random_generator()());
          const AbsPath gzip_path = macros::to_string(macros::SERVER_TEMP_STORAGE_DIR) + "/" +
                                    audio_id + macros::to_string(macros::COMPRESSED_ARCHIVE_EXT);

          bfs::create_directories(macros::SERVER_TEMP_STORAGE_DIR);
          std::ofstream output_file(gzip_path, std::ios::binary);
          output_file.write(req.body.data(), req.body.size());
          output_file.close();

          if (!bfs::exists(gzip_path) || bfs::file_size(gzip_path) == 0)
          {
            log::ERROR<ServerUpload>(LogMode::Async,
                                     "GZIP upload failed: File is empty or missing!");
            return crow::response(400, "GZIP upload failed");
          }

          if (helpers::extract_and_validate(gzip_path, audio_id))
          {
            bfs::remove(gzip_path);
            return crow::response(200, "Audio-ID: " + audio_id);
          }
          else
          {
            bfs::remove(gzip_path);
            return crow::response(400, macros::to_string(macros::SERVER_ERROR_400));
          }
        });

    // Download route fallback (GET /download/<string>)
    CROW_ROUTE(app, "/download/<string>/<string>/<string>")
      .methods(crow::HTTPMethod::GET)(
        [](const crow::request& req, const StorageOwnerID& ownerID, const StorageAudioID& audioID,
           const std::string& filename)
        {
          log::INFO<Server>(LogMode::Async, "Download request received for Audio-ID: {}", audioID);

          try
          {
            methods::DownloadManager dm(audioID, req);
            return dm.runDirect(ownerID,
                                filename); // Pass actual owner ID from your auth/session
          }
          catch (const std::exception& e)
          {
            log::ERROR<Server>(LogMode::Async, "Error handling download for '{}': {}", audioID,
                               e.what());
            crow::response res(500);
            res.set_header("Content-Type", "text/plain");
            res.write("Internal Server Error");
            return res;
          }
        });
  }
};

} // namespace libwavy::server
