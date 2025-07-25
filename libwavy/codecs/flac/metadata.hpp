#pragma once
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

#include <FLAC++/metadata.h>
#include <libwavy/common/types.hpp>
#include <libwavy/log-macros.hpp>
#include <sys/stat.h> // For file size

namespace libwavy::codecs
{

class FlacMetadataParser
{
public:
  struct FlacMetadata
  {
    double                                       bitrate;
    ui64                                         total_samples;
    ui32                                         sample_rate;
    ui32                                         bits_per_sample;
    ui32                                         channels;
    double                                       duration;
    ui64                                         file_size;
    std::string                                  vendor_string;
    std::unordered_map<std::string, std::string> tags;
  };

  static auto parse_metadata(const AbsPath& filename) -> FlacMetadata
  {
    FlacMetadata          metadata{};
    FLAC::Metadata::Chain chain;

    if (!chain.read(filename.c_str()))
    {
      log::ERROR<log::FLAC>("Failed to read FLAC metadata!");
      return metadata;
    }

    FLAC::Metadata::Iterator iter;
    iter.init(chain);
    do
    {
      FLAC::Metadata::Prototype* block = iter.get_block();

      // Extract audio stream information
      if (block->get_type() == FLAC__METADATA_TYPE_STREAMINFO)
      {
        auto* stream_info = dynamic_cast<FLAC::Metadata::StreamInfo*>(block);
        if (!stream_info)
        {
          log::ERROR<log::FLAC>("Failed to cast to StreamInfo.");
          return metadata;
        }

        metadata.total_samples   = stream_info->get_total_samples();
        metadata.sample_rate     = stream_info->get_sample_rate();
        metadata.bits_per_sample = stream_info->get_bits_per_sample();
        metadata.channels        = stream_info->get_channels();

        if (metadata.total_samples == 0 || metadata.sample_rate == 0)
        {
          log::ERROR<log::FLAC>("Invalid FLAC metadata values.");
          return metadata;
        }

        // Compute duration
        metadata.duration = static_cast<double>(metadata.total_samples) / metadata.sample_rate;

        // Get file size
        struct stat file_stat;
        if (stat(filename.c_str(), &file_stat) != 0)
        {
          log::ERROR<log::FLAC>("Failed to get file size.");
          return metadata;
        }
        metadata.file_size = file_stat.st_size;

        // Compute bitrate (bps)
        metadata.bitrate = (metadata.file_size * 8.0) / metadata.duration;
      }

      // Extract vendor string (encoder)
      if (block->get_type() == FLAC__METADATA_TYPE_VORBIS_COMMENT)
      {
        auto* vorbis = dynamic_cast<FLAC::Metadata::VorbisComment*>(block);
        if (!vorbis)
        {
          log::ERROR<log::FLAC>("Failed to cast to VorbisComment.");
          return metadata;
        }

        metadata.vendor_string =
          std::string(reinterpret_cast<const char*>(vorbis->get_vendor_string()));

        // Extract metadata tags
        for (size_t i = 0; i < vorbis->get_num_comments(); ++i)
        {
          FLAC::Metadata::VorbisComment::Entry entry = vorbis->get_comment(i);
          std::string                          key   = entry.get_field_name();
          std::string                          value = entry.get_field_value();
          metadata.tags[key]                         = value;
        }
      }

    } while (iter.next());

    return metadata;
  }
};

} // namespace libwavy::codecs
