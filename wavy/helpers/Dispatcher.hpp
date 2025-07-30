#pragma once

#include <libwavy/common/macros.hpp>
#include <libwavy/dispatch/entry.hpp>

// A neat wrapper for dispatcher that works right out of the box

auto dispatch(const IPAddr& server, const StorageOwnerID& nickname, const Directory& outputDir)
  -> int
{

  try
  {
    libwavy::dispatch::Dispatcher dispatcher(server, nickname, outputDir,
                                             macros::to_string(macros::MASTER_PLAYLIST));
    if (!dispatcher.process_and_upload())
    {
      libwavy::log::ERROR<libwavy::log::DISPATCH>("Upload process failed.");
      return WAVY_RET_FAIL;
    }

    libwavy::log::INFO<libwavy::log::DISPATCH>("Upload successful.");
    fs::remove_all(outputDir);
    fs::remove_all(macros::DISPATCH_ARCHIVE_REL_PATH);
    return WAVY_RET_SUC;
  }
  catch (const std::exception& e)
  {
    libwavy::log::ERROR<libwavy::log::DISPATCH>("Error during dispatch: {}", e.what());
    return WAVY_RET_SUC;
  }
}
