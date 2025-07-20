#include <libwavy/utils/audio/entry.hpp>

namespace libwavy::utils::audio
{

auto decodeAndPlay(GlobalState& gs, bool& flac_found, const RelPath& customAudioBackendLibPath)
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

} // namespace libwavy::utils::audio
