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

#include <functional>
#include <libwavy/common/state.hpp>
#include <libwavy/common/types.hpp>
#include <memory>

namespace libwavy::audio
{
class IAudioBackend
{
public:
  virtual ~IAudioBackend() = default;

  virtual auto initialize(const TotalDecodedAudioData& audioInput, bool isFlac,
                          int preferredSampleRate = 0, int preferredChannels = 0, int bitDepth = 16)
    -> bool = 0;

  virtual void play() = 0;

  [[nodiscard]] virtual auto name() const -> AudioBackendPluginName = 0;
};

using AudioBackendPtr = std::unique_ptr<IAudioBackend, std::function<void(IAudioBackend*)>>;

} // namespace libwavy::audio
