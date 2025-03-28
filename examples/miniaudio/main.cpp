#include "../../include/playback.hpp"
#include <iostream>

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
    AudioPlayer player(audioData, flac_found);
    player.play();
  }
  catch (const std::exception& e)
  {
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
  }

  return 0;
}
