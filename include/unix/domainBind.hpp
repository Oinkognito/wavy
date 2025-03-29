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


#include <cstring>
#include <fcntl.h>
#include <stdexcept>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "../libwavy-common/logger.hpp"

/*
 * @UnixSocketBind 
 * 
 * For now it just binds a UNIX domain socket for the server and cleans up after exit.
 *
 */

class UnixSocketBind
{
private:
  int         lock_fd_;
  int         server_fd_;
  std::string socket_path_;

public:
  UnixSocketBind(std::string socket_path)
      : socket_path_(std::move(socket_path)), lock_fd_(-1), server_fd_(-1)
  {
  }

  void ensure_single_instance()
  {
    struct sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    std::strncpy(addr.sun_path, socket_path_.c_str(), sizeof(addr.sun_path) - 1);

    lock_fd_ = socket(AF_UNIX, SOCK_STREAM, 0);
    if (lock_fd_ == -1)
    {
      throw std::runtime_error("Failed to create UNIX socket for locking");
    }

    if (bind(lock_fd_, (struct sockaddr*)&addr, sizeof(addr)) == -1)
    {
      close(lock_fd_);
      throw std::runtime_error("Another instance is already running!");
    }

    LOG_INFO << SERVER_LOG << "Lock acquired: " << socket_path_;
  }

  void cleanup()
  {
    if (lock_fd_ != -1)
    {
      close(lock_fd_);
      unlink(socket_path_.c_str());
      LOG_INFO << SERVER_LOG << "Lock file removed: " << socket_path_;
      lock_fd_ = -1;
    }
  }

  ~UnixSocketBind() { cleanup(); }
};
