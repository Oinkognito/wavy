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

#include "libwavy/db/db.h"
#if __cplusplus < 202002L
#error "Wavy-Server requires C++20 or later."
#endif

#include <libwavy/common/state.hpp>
#include <libwavy/db/entry.hpp>
#include <libwavy/server/server.hpp>

CREATE_WAVY_MINI_DB();

auto main() -> int
{
  try
  {
    INIT_WAVY_LOGGER_ALL();

    // always have fallback to trace for more detailed logs
    lwlog::set_log_level(libwavy::log::__TRACE__);

    libwavy::server::WavyServer server(WAVY_SERVER_PORT_NO, macros::to_string(macros::SERVER_CERT),
                                       macros::to_string(macros::SERVER_PRIVATE_KEY),
                                       storage::g_owner_audio_db);
    server.run();
  }
  catch (std::exception& e)
  {
    libwavy::log::ERROR<libwavy::log::SERVER>("Wavy Server Exception: {}", e.what());
  }

  return 0;
}
