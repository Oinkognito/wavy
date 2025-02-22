#include "../toml/toml_parser.hpp"
#include <iostream>

auto main() -> int
{
  auto parsed = parseAudioMetadata("../toml/sample.toml");

  std::cout << "Parsed metadata:" << std::endl;
  std::cout << "Path: " << parsed.path << std::endl;
  std::cout << "Audio type: " << parsed.audio_type << std::endl;
  std::cout << "Duration: " << parsed.duration << std::endl;
  std::cout << "Bitrate: " << parsed.bitrate << std::endl;
  std::cout << "Title: " << parsed.title << std::endl;
  std::cout << "Artist: " << parsed.artist << std::endl;
  std::cout << "Album: " << parsed.album << std::endl;
  std::cout << "Track: " << parsed.track.first << "/" << parsed.track.second << std::endl;
  std::cout << "Disc: " << parsed.disc.first << "/" << parsed.disc.second << std::endl;
  std::cout << "Copyright: " << parsed.copyright << std::endl;
  std::cout << "Genre: " << parsed.genre << std::endl;
  std::cout << "Comment: " << parsed.comment << std::endl;
  std::cout << "Album artist: " << parsed.album_artist << std::endl;
  std::cout << "TSRC: " << parsed.tsrc << std::endl;
  std::cout << "Encoder: " << parsed.encoder << std::endl;
  std::cout << "Encoded by: " << parsed.encoded_by << std::endl;
  std::cout << "Date: " << parsed.date << std::endl;

  std::cout << "TOML file parsed successfully!" << std::endl;
  return 0;
}
