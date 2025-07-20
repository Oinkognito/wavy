#pragma once
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

#include <archive.h>
#include <archive_entry.h>
#include <unistd.h>

#include <libwavy/server/session.hpp>

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

class WavyServer
{
public:
  WavyServer(boost::asio::io_context& io_context, boost::asio::ssl::context& ssl_context,
             short port)
      : m_acceptor(io_context, tcp::endpoint(tcp::v4(), port)), m_sslContext(ssl_context),
        m_signals(io_context, SIGINT, SIGTERM, SIGHUP),
        m_socketPath(macros::to_string(macros::SERVER_LOCK_FILE)), m_wavySocketBind(m_socketPath)

  {
    m_wavySocketBind.ensure_single_instance();
    LOG_INFO << SERVER_LOG << "Starting Wavy Server on port " << port;

    m_signals.async_wait(
      [this](boost::system::error_code /*ec*/, int /*signo*/)
      {
        LOG_INFO << SERVER_LOG << "Termination signal received. Cleaning up...";
        m_wavySocketBind.cleanup();
        std::exit(0);
      });

    start_accept();
  }

  ~WavyServer() { m_wavySocketBind.cleanup(); }

private:
  tcp::acceptor              m_acceptor;
  boost::asio::ssl::context& m_sslContext;
  boost::asio::signal_set    m_signals;
  FileName                   m_socketPath;
  unix::UnixSocketBind       m_wavySocketBind;

  void start_accept()
  {
    m_acceptor.async_accept(
      [this](boost::system::error_code ec, tcp::socket socket)
      {
        if (ec)
        {
          LOG_ERROR_ASYNC << SERVER_LOG << "Accept failed: " << ec.message();
          return;
        }

        IPAddr ip = socket.remote_endpoint().address().to_string();
        LOG_INFO_ASYNC << SERVER_LOG << "Accepted new connection from " << ip;

        auto session = std::make_shared<WavySession>(
          boost::asio::ssl::stream<tcp::socket>(std::move(socket), m_sslContext), ip);
        session->start();
        start_accept();
      });
  }
};

} // namespace libwavy::server
