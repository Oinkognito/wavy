#pragma once
/************************************************
 * Wavy Project - High-Fidelity Audio Streaming
 * ---------------------------------------------
 * 
 * Copyright (c) 2025 Oinkognito
 * All rights reserved.
 * 
 * This source code is part of the Wavy project, an advanced
 * local networking solution for high-quality audio streaming.
 * 
 * License:
 * This software is licensed under the BSD-3-Clause License.
 * You may use, modify, and distribute this software under
 * the conditions stated in the LICENSE file provided in the
 * project root.
 * 
 * Warranty Disclaimer:
 * This software is provided "AS IS," without any warranties
 * or guarantees, either expressed or implied, including but
 * not limited to fitness for a particular purpose.
 * 
 * Contributions:
 * Contributions to this project are welcome. By submitting 
 * code, you agree to license your contributions under the 
 * same BSD-3-Clause terms.
 * 
 * See LICENSE file for full details.
 ************************************************/

#include <autogen/audioConfig.h>
#include <libwavy/audio/plugin/entry.hpp>
#include <libwavy/common/state.hpp>
#include <libwavy/ffmpeg/decoder/entry.hpp>

auto decodeAndPlay(GlobalState& gs, bool& flac_found, const RelPath& customAudioBackendLibPath = "")
  -> bool
{
  auto segments = gs.getAllSegments();

  if (segments.empty())
  {
    LOG_ERROR << DECODER_LOG << "No transport stream segments provided";
    return false;
  }

  LOG_INFO << "Decoding transport stream segments...";

  libwavy::ffmpeg::MediaDecoder decoder;
  TotalDecodedAudioData         decoded_audio;
  if (!decoder.decode(segments, decoded_audio))
  {
    LOG_ERROR << DECODER_LOG << "Decoding failed";
    return false;
  }

  if (gNumAudioBackends == 0)
  {
    LOG_ERROR << PLUGIN_LOG << "No audio backend plugins found! Exiting cleanly...";
    return false;
  }

  const RelPath audioBackendLibPath =
    std::string(WAVY_AUDIO_BACKEND_PLUGIN_OUTPUT_PATH) + "/" +
    (customAudioBackendLibPath.empty() ? gAudioBackends[0].plugin_path : customAudioBackendLibPath);

  LOG_INFO << RECEIVER_LOG << "Given Audio Backend Plugin: '" << audioBackendLibPath;

  try
  {
    // load audio backend plugin here
    LOG_INFO << PLUGIN_LOG << "Loading audio backend plugin...";
    libwavy::audio::AudioBackendPtr backend;
    backend = libwavy::audio::plugin::WavyAudioBackend::load(audioBackendLibPath);
    if (backend->initialize(decoded_audio, flac_found))
    {
      LOG_TRACE << PLUGIN_LOG << "Loaded: " << backend->name();
      backend->play();
    }
    else
    {
      LOG_ERROR << PLUGIN_LOG << "Error while loading plugin: " << audioBackendLibPath;
      return false;
    }
  }
  catch (const std::exception& e)
  {
    LOG_ERROR << AUDIO_LOG << "Audio playback error: " << e.what();
    return false;
  }

  return true;
}
