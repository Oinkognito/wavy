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

#include <libwavy/common/macros.hpp>
#include <libwavy/common/types.hpp>
#include <libwavy/dispatch/entry.hpp>

// A neat wrapper for dispatcher that works right out of the box

auto main(int argc, char* argv[]) -> int
{
  INIT_WAVY_LOGGER();

  if (argc < 4)
  {
    lwlog::ERROR<_>("{} <server-ip> <nickname> <output-dir>", argv[0]);
    lwlog::ERROR<_>("Payload directory refers to the directory that contains the desired playlists "
                    "and transport segments.");
    return WAVY_RET_FAIL;
  }

  // Add your custom logic to validate these cmd-line args if needed...
  const IPAddr         server   = IPAddr(argv[1]);
  const StorageOwnerID nickname = StorageOwnerID(argv[2]);
  const Directory      dir      = Directory(argv[3]);

  try
  {
    libwavy::dispatch::Dispatcher dispatcher(server, nickname, dir,
                                             macros::to_string(macros::MASTER_PLAYLIST));
    if (!dispatcher.process_and_upload())
    {
      lwlog::ERROR<lwlog::DISPATCH>("Upload process failed!");
      return WAVY_RET_FAIL;
    }

    lwlog::INFO<lwlog::DISPATCH>("Upload successful!!");
    return WAVY_RET_SUC;
  }
  catch (const std::exception& e)
  {
    lwlog::ERROR<_>("[Main] Error: {}", e.what());
    return WAVY_RET_SUC;
  }

  return WAVY_RET_SUC;
}
