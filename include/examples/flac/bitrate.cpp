#include "../../codecs/flac/bitrate.hpp"
#include <iostream>
#include <sys/stat.h> // For file size

auto main(int argc, char* argv[]) -> int
{
  if (argc < 2)
  {
    std::cerr << "Usage: " << argv[0] << " <flac_file>\n";
    return 1;
  }

  FLACBitrateCalculator fbc;

  double bitrate = fbc.get_flac_bitrate(argv[1]);
  if (bitrate > 0)
  {
    std::cout << "FLAC Bitrate: " << bitrate / 1000.0 << " kbps\n";
  }
  else
  {
    std::cerr << "Failed to calculate bitrate.\n";
  }

  return 0;
}
