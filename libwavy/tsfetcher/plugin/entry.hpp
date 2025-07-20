#pragma once
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

#include <autogen/config.h>
#include <dlfcn.h>
#include <functional>
#include <libwavy/common/types.hpp>
#include <libwavy/logger.hpp>
#include <libwavy/tsfetcher/interface.hpp>
#include <string>

namespace libwavy::fetch::plugin
{
class FetcherFactory
{
public:
  static auto create(const AbsPath& plugin_path, const IPAddr& server) -> SegmentFetcherPtr
  {
    using FetcherCreateFunc = ISegmentFetcher* (*)(const char*);

    LOG_INFO << PLUGIN_LOG << "Found plugin path: " << WAVY_FETCHER_PLUGIN_OUTPUT_PATH;

    LOG_INFO << PLUGIN_LOG << "Attempting to load plugin from: " << plugin_path;

    // Load the plugin shared object
    void* handle = dlopen(plugin_path.c_str(), RTLD_LAZY);
    if (!handle)
    {
      throw std::runtime_error(std::string("Failed to load plugin: ") + dlerror());
    }

    LOG_INFO << PLUGIN_LOG << "Plugin loaded successfully. Resolving symbols...";

    // Load the create function from the plugin
    auto create_fn = reinterpret_cast<FetcherCreateFunc>(dlsym(handle, "create_fetcher_with_arg"));
    if (!create_fn)
    {
      dlclose(handle);
      throw std::runtime_error("Failed to load symbol: create_fetcher_with_arg");
    }

    LOG_TRACE << PLUGIN_LOG << "Symbol 'create_fetcher_with_arg' resolved successfully.";
    LOG_TRACE << PLUGIN_LOG << "Creating fetcher instance with server: " << server;

    // Create the fetcher instance
    ISegmentFetcher* fetcher_ptr = create_fn(server.c_str());

    if (!fetcher_ptr)
    {
      dlclose(handle);
      throw std::runtime_error("Fetcher creation returned null.");
    }

    LOG_INFO << PLUGIN_LOG << "Fetcher instance created successfully.";

    // Wrap the pointer in a std::unique_ptr with a custom deleter
    return {fetcher_ptr, [handle](ISegmentFetcher* ptr)
            {
              LOG_TRACE << PLUGIN_LOG << "Destroying fetcher instance and unloading plugin.";
              delete ptr;
              dlclose(handle);
            }};
  }
};
} // namespace libwavy::fetch::plugin
