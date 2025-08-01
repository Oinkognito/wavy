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

#include <cstring>
#include <fcntl.h>
#include <stdexcept>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <libwavy/common/types.hpp>
#include <libwavy/log-macros.hpp>

/*
 * @UnixSocketBind 
 * 
 * For now it just binds a UNIX domain socket for the server and cleans up after exit.
 *
 */

#define LOCK_REMOVED_FD -1

namespace libwavy::unix
{

class UnixSocketBind
{
private:
  int     m_lockFD{};
  int     m_serverFD{};
  AbsPath m_socketPath;

public:
  UnixSocketBind(std::string socket_path)
      : m_socketPath(std::move(socket_path)), m_lockFD(-1), m_serverFD(-1)
  {
  }

  void EnsureSingleInstance()
  {
    struct sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    std::strncpy(addr.sun_path, m_socketPath.c_str(), sizeof(addr.sun_path) - 1);

    m_lockFD = socket(AF_UNIX, SOCK_STREAM, 0);
    if (m_lockFD == -1)
    {
      throw std::runtime_error("Failed to create UNIX socket for locking");
    }

    if (bind(m_lockFD, (struct sockaddr*)&addr, sizeof(addr)) == -1)
    {
      close(m_lockFD);
      throw std::runtime_error("Another instance is already running!");
    }

    log::INFO<log::UNIX>("Lock acquired: {}", m_socketPath);
  }

  void cleanup()
  {
    if (m_lockFD != -1)
    {
      close(m_lockFD);
      unlink(m_socketPath.c_str());
      log::INFO<log::UNIX>("Lock file removed: {}", m_socketPath);
      m_lockFD = LOCK_REMOVED_FD;
    }
  }

  ~UnixSocketBind() { cleanup(); }
};

} // namespace libwavy::unix
