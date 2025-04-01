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
#include <libwavy/codecs/flac/metadata.hpp>
#include <sys/stat.h> // For file size

auto main(int argc, char* argv[]) -> int
{
  if (argc > 1)
  {
    std::string file     = argv[1];
    auto        metadata = libwavy::codecs::FlacMetadataParser::parse_metadata(file);

    std::cout << "Bitrate:         " << metadata.bitrate << " bps\n";
    std::cout << "Total Samples:   " << metadata.total_samples << "\n";
    std::cout << "Sample Rate:     " << metadata.sample_rate << " Hz\n";
    std::cout << "Bits Per Sample: " << metadata.bits_per_sample << "\n";
    std::cout << "Channels:        " << metadata.channels << "\n";
    std::cout << "Duration:        " << metadata.duration << " sec\n";
    std::cout << "File Size:       " << metadata.file_size << " bytes\n";
    std::cout << "Vendor String:   " << metadata.vendor_string << "\n";

    std::cout << "\n--------- Tags: ----------\n";
    for (const auto& [key, value] : metadata.tags)
    {
      std::cout << "  " << key << ": " << value << "\n";
    }
  }
  else
  {
    std::cerr << argv[0] << " <input-flac-file> " << std::endl;
  }

  return 0;
}
