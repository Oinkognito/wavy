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

#include <archive.h>
#include <archive_entry.h>

#define CROW_ENABLE_SSL
#include <crow.h>
#include <crow/middlewares/cookie_parser.h>

#include <libwavy/common/api/entry.hpp>
#include <libwavy/common/network/routes.h>
#include <libwavy/server/health.hpp>
#include <libwavy/server/methods/download.hpp>
#include <libwavy/server/methods/owners.hpp>
#include <libwavy/server/metrics.hpp>
#include <libwavy/server/request-timer.hpp>
#include <libwavy/toml/toml_parser.hpp>
#include <libwavy/unix/domainBind.hpp>
#include <libwavy/utils/math/entry.hpp>
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

namespace libwavy::server
{

class WAVY_API WavyServer
{
public:
  WavyServer(short port, AbsPath serverCert, AbsPath serverKey)
      : m_socketPath(macros::to_string(macros::SERVER_LOCK_FILE)), m_wavySocketBind(m_socketPath),
        m_port(port), m_serverCert(std::move(serverCert)), m_serverKey(std::move(serverKey)),
        m_shutdown_requested(false), m_metrics(std::make_unique<Metrics>()),
        m_ownerManager(*m_metrics)
  {
    m_wavySocketBind.EnsureSingleInstance();
    log::INFO<Server>("Starting Wavy Server on port {}", port);

    // Set up signal handlers for graceful shutdown
    std::signal(SIGINT, [](int signo) { get_instance()->request_shutdown(signo); });
    std::signal(SIGTERM, [](int signo) { get_instance()->request_shutdown(signo); });
    std::signal(SIGHUP, [](int signo) { get_instance()->request_shutdown(signo); });

    // Store instance for signal handling
    s_instance = this;
  }

  ~WavyServer()
  {
    if (!m_shutdown_requested)
    {
      request_shutdown(SIGTERM);
    }

    m_wavySocketBind.cleanup();
  }

  void run()
  {
    try
    {
      m_app.get_middleware<crow::CookieParser>();

      setup_routes(m_app);
      setup_health_routes(m_app);
      setup_metrics_routes(m_app);

      log::INFO<Server>("Server configured successfully, starting listeners...");

      m_app.port(m_port)
        .ssl_file(m_serverCert, m_serverKey)
        .multithreaded()
        .timeout(30) // 30 second timeout
        .loglevel(crow::LogLevel::DEBUG)
        .server_name("WavyServer")
        .run();
    }
    catch (const std::exception& e)
    {
      log::ERROR<Server>("Fatal error starting server: {}", e.what());
      throw;
    }
  }

  void request_shutdown(int signo)
  {
    log::INFO<Server>("Shutdown signal ({}) received. Initiating graceful shutdown...", signo);
    m_shutdown_requested = true;
    m_shutdown_cv.notify_all();
  }

private:
  FileName                      m_socketPath;
  unix::UnixSocketBind          m_wavySocketBind;
  short                         m_port;
  AbsPath                       m_serverCert, m_serverKey;
  crow::App<crow::CookieParser> m_app;

  // Shutdown management
  std::atomic<bool>       m_shutdown_requested;
  std::condition_variable m_shutdown_cv;
  std::mutex              m_shutdown_mutex;

  // Metrics
  std::unique_ptr<Metrics> m_metrics;
  methods::OwnerManager    m_ownerManager;

  // Singleton for signal handling
  static WavyServer* s_instance;
  static auto        get_instance() -> WavyServer* { return s_instance; }

  template <typename CrowApp> void setup_health_routes(CrowApp& app)
  {
    // Health check endpoint
    CROW_ROUTE(app, "/health")
      .methods(crow::HTTPMethod::GET)(
        [this]()
        {
          RequestTimer timer(*m_metrics);

          auto health = HealthChecker::check_system_health();

          std::ostringstream response;
          response << "{\n";
          response << R"(  "status": ")" << health.status_message << "\",\n";
          response << "  \"healthy\": " << (health.is_healthy ? "true" : "false") << ",\n";
          response << "  \"checks\": {\n";

          bool first = true;
          for (const auto& check : health.checks)
          {
            if (!first)
              response << ",\n";
            response << "    \"" << check.first << "\": \"" << check.second << "\"";
            first = false;
          }

          response << "\n  }\n}";

          int status_code = health.is_healthy ? 200 : 503;
          if (status_code == 200)
            timer.mark_success();
          else
            timer.mark_failure();

          crow::response res(status_code, response.str());
          res.set_header("Content-Type", "application/json");
          return res;
        });
  }

  template <typename CrowApp> void setup_metrics_routes(CrowApp& app)
  {
    // Metrics endpoint (Prometheus-style)
    CROW_ROUTE(app, "/metrics")
      .methods(crow::HTTPMethod::GET)(
        [this]()
        {
          RequestTimer timer(*m_metrics);

          auto body = libwavy::server::MetricsSerializer::to_prometheus_format(*m_metrics);

          timer.mark_success();

          crow::response res(200, body);
          res.set_header("Content-Type", "text/plain; version=0.0.4");
          return res;
        });
  }

  template <typename CrowApp> void setup_routes(CrowApp& app)
  {
    CROW_ROUTE(app, routes::SERVER_PATH_PING)
      .methods(crow::HTTPMethod::GET)(
        [this]()
        {
          RequestTimer timer(*m_metrics);
          log::INFO<Server>("Sending pong to client...");
          timer.mark_success();
          return crow::response(200, macros::to_string(macros::SERVER_PONG_MSG));
        });

    CROW_ROUTE(app, routes::SERVER_PATH_OWNERS)
      .methods(crow::HTTPMethod::GET)([this]() { return m_ownerManager.list_owners(); });

    // Audio Metadata Listing (GET /audio/info)
    CROW_ROUTE(app, routes::SERVER_PATH_AUDIO_INFO)
      .methods(crow::HTTPMethod::GET)([this]() { return m_ownerManager.list_audio_info(); });

    // File upload (POST /upload)
    CROW_ROUTE(app, routes::SERVER_PATH_TOML_UPLOAD)
      .methods(crow::HTTPMethod::POST)([this](const crow::request& req)
                                       { return m_ownerManager.handle_upload(req); });

    CROW_ROUTE(app, "/stream/<string>/<string>/<string>")
    (
      [this](crow::response& res, const StorageOwnerID& owner_id, const StorageAudioID& audio_id,
             const std::string& filename)
      {
        const bfs::path file_path =
          bfs::path(macros::SERVER_STORAGE_DIR) / owner_id / audio_id / filename;

        log::INFO<Server>("Attempting to start stream for '{}'", file_path);

        if (!bfs::exists(file_path))
        {
          res.code = 404;
          res.end("File not found");
          return;
        }

        res.set_header("Content-Type", "application/octet-stream");
        res.set_header("Transfer-Encoding", "chunked");

        std::thread(
          [file_path, res = std::move(res)]() mutable
          {
            int fd = ::open(file_path.string().c_str(), O_RDONLY);
            if (fd < 0)
            {
              res.code = 500;
              res.end("Internal Server Error");
              return;
            }

            constexpr size_t  CHUNK = 64 * 1024;
            std::vector<char> buf(CHUNK);
            ssize_t           n;
            while ((n = ::read(fd, buf.data(), buf.size())) > 0)
            {
              res.write(std::string(buf.data(), n));
            }

            ::close(fd);
            res.end();
          })
          .detach();
      });

    // File download (GET /download/<owner-id>/<audio-id>/<filename>)
    CROW_ROUTE(app, routes::SERVER_PATH_DOWNLOAD)
      .methods(crow::HTTPMethod::GET)(
        [this](const crow::request& req, const StorageOwnerID& ownerID,
               const StorageAudioID& audioID, const std::string& filename)
        {
          log::INFO<Server>(LogMode::Async,
                            "Download request received for Audio-ID: {} by Owner: {}", audioID,
                            ownerID);

          methods::DownloadManager dm(*m_metrics, ownerID, audioID, req);
          auto                     response = dm.runDirect(filename);

          return response;
        });

    CROW_ROUTE(app, routes::SERVER_PATH_DELETE)
      .methods(crow::HTTPMethod::DELETE)(
        [this](const crow::request& req, const StorageOwnerID& ownerID,
               const StorageAudioID& audioID)
        {
          log::INFO<Server>(LogMode::Async, "Delete request by owner '{}' for Audio-ID: {}",
                            ownerID, audioID);
          return m_ownerManager.handle_delete(req, ownerID, audioID);
        });

    CROW_ROUTE(app, "/owner/metrics/<string>")
      .methods(crow::HTTPMethod::GET)(
        [this](const crow::request& req, const std::string& ownerID)
        {
          RequestTimer timer(*m_metrics);

          auto result = m_metrics->get_owner_metrics(ownerID);
          if (!result.has_value())
          {
            timer.mark_failure();
            return crow::response(404, "Owner not found");
          }

          timer.mark_success();
          crow::json::wvalue json_res;
          json_res["owner_id"]      = ownerID;
          json_res["uploads"]       = result->get().uploads.load();
          json_res["downloads"]     = result->get().downloads.load();
          json_res["deletes"]       = result->get().deletes.load();
          json_res["songs_count"]   = result->get().songs_count.load();
          json_res["storage_bytes"] = result->get().storage_bytes.load();

          return crow::response{json_res};
        });
  }
};

// Static instance for signal handling
WavyServer* WavyServer::s_instance = nullptr;

} // namespace libwavy::server
