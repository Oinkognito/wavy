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

#include <iostream>
#include <libwavy/toml/toml_parser.hpp>

using namespace std;

auto main(int argc, char* argv[]) -> int
{
  if (argc != 2)
  {
    cerr << "Usage: " << argv[0] << " <metadata.toml>" << endl;
    return EXIT_FAILURE;
  }

  string tomlFilePath = argv[1];

  try
  {
    // Parse metadata from TOML file
    AudioMetadata metadata = parseAudioMetadata(tomlFilePath);

    // Display extracted metadata
    cout << "Audio Metadata Extracted:\n";
    cout << "Bitrate: " << metadata.bitrate << " kbps\n";
    cout << "Duration: " << metadata.duration << " seconds\n";
    cout << "Path: " << metadata.path << "\n";
    cout << "File Format: " << metadata.file_format << "\n";
    cout << "File Format (Long): " << metadata.file_format_long << "\n";

    cout << "\nMetadata:\n";
    cout << "Title: " << metadata.title << "\n";
    cout << "Artist: " << metadata.artist << "\n";
    cout << "Album: " << metadata.album << "\n";
    cout << "Track: " << metadata.track.first << "/" << metadata.track.second << "\n";
    cout << "Disc: " << metadata.disc.first << "/" << metadata.disc.second << "\n";
    cout << "Genre: " << metadata.genre << "\n";
    cout << "Copyright: " << metadata.copyright << "\n";
    cout << "Comment: " << metadata.comment << "\n";
    cout << "Album Artist: " << metadata.album_artist << "\n";
    cout << "TSRC: " << metadata.tsrc << "\n";
    cout << "Encoder: " << metadata.encoder << "\n";
    cout << "Encoded By: " << metadata.encoded_by << "\n";
    cout << "Date: " << metadata.date << "\n";

    cout << "\nAudio Stream Metadata:\n";
    cout << "Codec: " << metadata.audio_stream.codec << "\n";
    cout << "Type: " << metadata.audio_stream.type << "\n";
    cout << "Bitrate: " << metadata.audio_stream.bitrate << " kbps\n";
    cout << "Sample Rate: " << metadata.audio_stream.sample_rate << " Hz\n";
    cout << "Channels: " << metadata.audio_stream.channels << "\n";
    cout << "Channel Layout: " << metadata.audio_stream.channel_layout << "\n";
    cout << "Sample Format: " << metadata.audio_stream.sample_format << "\n";

    cout << "\nVideo Stream Metadata:\n";
    cout << "Codec: " << metadata.video_stream.codec << "\n";
    cout << "Type: " << metadata.video_stream.type << "\n";
    cout << "Bitrate: " << metadata.video_stream.bitrate << " kbps\n";
    cout << "Sample Rate: " << metadata.video_stream.sample_rate << " Hz\n";
    cout << "Channels: " << metadata.video_stream.channels << "\n";
    cout << "Channel Layout: " << metadata.video_stream.channel_layout << "\n";
    cout << "Sample Format: " << metadata.video_stream.sample_format << "\n";
  }
  catch (const exception& e)
  {
    cerr << "Error parsing metadata: " << e.what() << endl;
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
