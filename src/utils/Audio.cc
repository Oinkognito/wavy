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

#include <libwavy/utils/audio/entry.hpp>

using Decoder = libwavy::log::DECODER;

namespace libwavy::utils::audio
{

auto decodeAndPlay(GlobalState& gs, bool& flac_found, const RelPath& customAudioBackendLibPath)
  -> bool
{
  auto segments = gs.getAllSegments();

  if (segments.empty())
  {
    log::ERROR<Decoder>("No transport stream segments provided!");
    return false;
  }

  log::INFO<Decoder>("Decoding transport stream segments...");

  libwavy::ffmpeg::MediaDecoder decoder;
  TotalDecodedAudioData         decoded_audio;
  if (!decoder.decode(segments, decoded_audio))
  {
    log::ERROR<Decoder>("Decoding failed!!! Check callback logs for more info.");
    return false;
  }

  if (gNumAudioBackends == 0)
  {
    log::ERROR<Decoder>("No audio backend plugins found! Exiting cleanly...");
    return false;
  }

  const RelPath audioBackendLibPath =
    RelPath(WAVY_AUDIO_BACKEND_PLUGIN_OUTPUT_PATH) + "/" +
    (customAudioBackendLibPath.empty() ? gAudioBackends[0].plugin_path : customAudioBackendLibPath);

  log::INFO<log::CLIENT>("Given Audio Backend Plugin: '{}'", audioBackendLibPath);

  try
  {
    // load audio backend plugin here
    log::INFO<log::PLUGIN>("Loading audio backend plugin...");
    libwavy::audio::AudioBackendPtr backend;
    backend = libwavy::audio::plugin::WavyAudioBackend::load(audioBackendLibPath);
    if (backend->initialize(decoded_audio, flac_found))
    {
      log::TRACE<log::PLUGIN>("Loaded: '{}'!", backend->name());
      backend->play();
    }
    else
    {
      log::ERROR<log::PLUGIN>("Error while loading plugin: {}", audioBackendLibPath);
      return false;
    }
  }
  catch (const std::exception& e)
  {
    log::ERROR<log::AUDIO>("Audio playback error: {}", e.what());
    return false;
  }

  return true;
}

} // namespace libwavy::utils::audio
