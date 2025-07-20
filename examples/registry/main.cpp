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

#include <iostream>
#include <libwavy/common/types.hpp>
#include <libwavy/registry/entry.hpp>

auto main(int argc, char* argv[]) -> int
{
  if (argc < 4)
  {
    std::cerr << "Usage: " << argv[0] << " <input_audio_file> <nickname> <output_toml_file>\n";
    return 1;
  }

  RelPath        inputFile  = argv[1];
  StorageOwnerID nickname   = argv[2];
  RelPath        outputFile = argv[3];

  libwavy::registry::RegisterAudio parser(inputFile, nickname,
                                          {}); // just give empty vector for found bitrates
  if (!parser.parse())
  {
    std::cerr << "Failed to parse audio file.\n";
    return 1;
  }

  parser.exportToTOML(outputFile);
  std::cout << "TOML metadata exported to " << outputFile << std::endl;

  avformat_network_deinit(); // Cleanup FFmpeg network components

  return 0;
}
