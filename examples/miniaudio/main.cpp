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

#include <iostream>
#include <libwavy/playback.hpp>

auto main() -> int
{
  // Read Audio data from standard input
  std::vector<unsigned char> audioData((std::istreambuf_iterator<char>(std::cin)),
                                       std::istreambuf_iterator<char>());
  if (audioData.empty())
  {
    std::cerr << "No Audio input received from STDIN" << std::endl;
    return 1;
  }

  bool flac_found = false;

  try
  {
    libwavy::audio::AudioPlayer player(audioData, flac_found);
    player.play();
  }
  catch (const std::exception& e)
  {
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
  }

  return 0;
}
