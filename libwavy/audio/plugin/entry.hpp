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

#include <autogen/config.h>
#include <dlfcn.h>
#include <functional>
#include <libwavy/audio/interface.hpp> // Defines IAudioBackend
#include <libwavy/logger.hpp>
#include <memory>
#include <stdexcept>
#include <string>

namespace libwavy::audio::plugin
{
class WavyAudioBackend
{
public:
  static auto load(const std::string& plugin_path)
    -> std::unique_ptr<IAudioBackend, std::function<void(IAudioBackend*)>>
  {
    using BackendCreateFunc = IAudioBackend* (*)();
    using MetadataFunc      = const char* (*)();

    LOG_INFO << PLUGIN_LOG << "Found plugin path: " << WAVY_AUDIO_BACKEND_PLUGIN_OUTPUT_PATH;

    LOG_INFO << PLUGIN_LOG << "Attempting to load plugin from: " << plugin_path;

    void* handle = dlopen(plugin_path.c_str(), RTLD_LAZY);
    if (!handle)
    {
      LOG_ERROR << PLUGIN_LOG << "Failed to load audio plugin: " << dlerror();
      throw std::runtime_error(std::string("Failed to load audio plugin: ") + dlerror());
    }

    LOG_INFO << PLUGIN_LOG << "Audio Backend Plugin loaded. Resolving symbols...";

    auto create_fn = reinterpret_cast<BackendCreateFunc>(dlsym(handle, "create_audio_backend"));
    auto meta_fn   = reinterpret_cast<MetadataFunc>(dlsym(handle, "get_plugin_metadata"));

    if (!create_fn || !meta_fn)
    {
      dlclose(handle);
      throw std::runtime_error("Required symbols not found in plugin.");
    }

    const char* metadata = meta_fn();
    LOG_INFO << PLUGIN_LOG << "Loaded audio backend plugin: " << metadata;

    IAudioBackend* backend_ptr = create_fn();
    if (!backend_ptr)
    {
      dlclose(handle);
      throw std::runtime_error("Audio backend creation failed.");
    }

    return {backend_ptr, [handle](IAudioBackend* ptr)
            {
              LOG_TRACE << AUDIO_LOG << "Destroying audio backend and unloading plugin.";
              delete ptr;
              dlclose(handle);
            }};
  }
};
} // namespace libwavy::audio::plugin
