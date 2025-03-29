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

#include "../../include/registry.hpp"
#include <iostream>

auto main(int argc, char* argv[]) -> int
{
  if (argc < 3)
  {
    std::cerr << "Usage: " << argv[0] << " <input_audio_file> <output_toml_file>\n";
    return 1;
  }

  std::string inputFile  = argv[1];
  std::string outputFile = argv[2];

  avformat_network_init(); // Initialize FFmpeg network components

  AudioParser parser(inputFile);
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
