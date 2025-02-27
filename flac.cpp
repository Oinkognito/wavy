#include "flac.h"

auto main() -> int
{
  if (!decode_audio("audio.raw"))
  {
    std::cerr << "Failed to decode audio\n";
    return 1;
  }

  play_audio();
  return 0;
}
