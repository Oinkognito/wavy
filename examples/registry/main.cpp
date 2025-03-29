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
