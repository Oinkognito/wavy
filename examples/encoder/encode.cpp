#include "../../include/libwavy-ffmpeg/encoder/entry.hpp"

auto main(int argc, char* argv[]) -> int
{
  logger::init_logging();
  // Register all codecs and formats (not needed in newer FFmpeg versions but included for compatibility)
  libwavy::ffmpeg::Encoder enc;

  if (argc != 4)
  {
    std::cout << "Usage: " << argv[0] << " <input-file> <output-file> <bitrate-in-bits/sec>"
              << std::endl;
    std::cout << "Example: " << argv[0] << " input.wav output.mp3 128000" << std::endl;
    return 1;
  }

  const char* input_file  = argv[1];
  const char* output_file = argv[2];
  int         bitrate;

  try
  {
    bitrate = std::stoi(argv[3]);
  }
  catch (const std::exception& e)
  {
    std::cerr << "Error: Bitrate must be a valid integer" << std::endl;
    return 1;
  }

  if (bitrate <= 0)
  {
    std::cerr << "Error: Bitrate must be positive" << std::endl;
    return 1;
  }

  int result = enc.transcode_mp3(input_file, output_file, bitrate);
  return (result < 0) ? 1 : 0;
}
