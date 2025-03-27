#include "../../codecs/flac/metadata.hpp"
#include <iostream>
#include <sys/stat.h> // For file size

auto main(int argc, char* argv[]) -> int
{
  if (argc > 1)
  {
    std::string file     = argv[1];
    auto        metadata = FlacMetadataParser::parse_metadata(file);

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
