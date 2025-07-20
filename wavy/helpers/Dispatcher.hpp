#pragma once

#include <libwavy/common/macros.hpp>
#include <libwavy/dispatch/entry.hpp>

// A neat wrapper for dispatcher that works right out of the box

auto dispatch(const IPAddr& server, const StorageOwnerID& nickname, const Directory& dir) -> int
{

  try
  {
    libwavy::dispatch::Dispatcher dispatcher(server, nickname, dir,
                                             macros::to_string(macros::MASTER_PLAYLIST));
    if (!dispatcher.process_and_upload())
    {
      LOG_ERROR << DISPATCH_LOG << "Upload process failed.";
      return WAVY_RET_FAIL;
    }

    LOG_INFO << DISPATCH_LOG << "Upload successful.";
    return WAVY_RET_SUC;
  }
  catch (const std::exception& e)
  {
    LOG_ERROR << DISPATCH_LOG << "Error: " << e.what();
    return WAVY_RET_SUC;
  }
}
