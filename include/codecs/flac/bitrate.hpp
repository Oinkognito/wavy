#pragma once

#include "../../logger.hpp"
#include <FLAC++/metadata.h>
#include <sys/stat.h> // For file size

class FLACBitrateCalculator
{
public:
  static auto get_flac_bitrate(const std::string& filename) -> double
  {
    FLAC::Metadata::Chain chain;
    if (!chain.read(filename.c_str()))
    {
      LOG_ERROR << "[FLAC] Failed to read FLAC metadata.\n";
      return -1;
    }

    FLAC::Metadata::Iterator iter;
    iter.init(chain);
    do
    {
      FLAC::Metadata::Prototype* block = iter.get_block();
      if (block->get_type() == FLAC__METADATA_TYPE_STREAMINFO)
      {
        auto* stream_info = dynamic_cast<FLAC::Metadata::StreamInfo*>(block);
        if (!stream_info)
        {
          LOG_ERROR << "[FLAC] Failed to cast to StreamInfo.\n";
          return -1;
        }

        uint64_t total_samples = stream_info->get_total_samples();
        uint32_t sample_rate   = stream_info->get_sample_rate();

        if (total_samples == 0 || sample_rate == 0)
        {
          LOG_ERROR << "[FLAC] Invalid FLAC metadata values.\n";
          return -1;
        }

        // Compute duration in seconds
        double duration = static_cast<double>(total_samples) / sample_rate;

        // Get file size in bytes
        struct stat file_stat;
        if (stat(filename.c_str(), &file_stat) != 0)
        {
          LOG_ERROR << "[FLAC] Failed to get file size.\n";
          return -1;
        }
        uint64_t file_size = file_stat.st_size;

        // Compute bitrate (bits per second)
        double bitrate = (file_size * 8.0) / duration;
        return bitrate;
      }
    } while (iter.next());

    LOG_ERROR << "[FLAC] StreamInfo block not found.\n";
    return -1;
  }
};
