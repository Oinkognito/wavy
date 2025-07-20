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

#include <libwavy/codecs/flac/metadata.hpp>
#include <libwavy/common/types.hpp>
#include <libwavy/logger.hpp>

auto main(int argc, char* argv[]) -> int
{
  libwavy::log::init_logging();

  if (argc > 1)
  {
    const RelPath file     = argv[1];
    auto          metadata = libwavy::codecs::FlacMetadataParser::parse_metadata(file);

    LOG_INFO << "Bitrate:         " << metadata.bitrate << " bps";
    LOG_INFO << "Total Samples:   " << metadata.total_samples;
    LOG_INFO << "Sample Rate:     " << metadata.sample_rate << " Hz";
    LOG_INFO << "Bits Per Sample: " << metadata.bits_per_sample;
    LOG_INFO << "Channels:        " << metadata.channels;
    LOG_INFO << "Duration:        " << metadata.duration << " secs";
    LOG_INFO << "File Size:       " << metadata.file_size << " bytes";
    LOG_INFO << "Vendor String:   " << metadata.vendor_string;

    LOG_INFO << "--------- Tags: ----------";
    for (const auto& [key, value] : metadata.tags)
    {
      LOG_INFO << "  " << key << ": " << value;
    }
  }
  else
  {
    LOG_ERROR << argv[0] << " <input-flac-file> ";
  }

  return 0;
}
