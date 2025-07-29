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

auto main(int argc, char* argv[]) -> int
{
  INIT_WAVY_LOGGER_ALL();

  if (argc > 1)
  {
    const RelPath file     = argv[1];
    auto          metadata = libwavy::codecs::FlacMetadataParser::parse_metadata(file);

    lwlog::INFO<lwlog::FLAC>("Bitrate:         {} bps", metadata.bitrate);
    lwlog::INFO<lwlog::FLAC>("Total Samples:   {}", metadata.total_samples);
    lwlog::INFO<lwlog::FLAC>("Sample Rate:     {} Hz", metadata.sample_rate);
    lwlog::INFO<lwlog::FLAC>("Bits Per Sample: {}", metadata.bits_per_sample);
    lwlog::INFO<lwlog::FLAC>("Channels:        {}", metadata.channels);
    lwlog::INFO<lwlog::FLAC>("Duration:        {} secs", metadata.duration);
    lwlog::INFO<lwlog::FLAC>("File Size:       {} bytes", metadata.file_size);
    lwlog::INFO<lwlog::FLAC>("Vendor String:   {}", metadata.vendor_string);

    lwlog::INFO<lwlog::FLAC>("--------- Tags: ----------");
    for (const auto& [key, value] : metadata.tags)
    {
      lwlog::INFO<lwlog::FLAC>("  {}: {}", key, value);
    }
  }
  else
  {
    lwlog::ERROR<lwlog::FLAC>("{} <input-flac-file>", argv[0]);
  }

  return 0;
}
