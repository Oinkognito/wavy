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

#include <libwavy/common/macros.hpp>
#include <libwavy/common/state.hpp>
#include <libwavy/common/types.hpp>
#include <libwavy/ffmpeg/misc/metadata.hpp>
#include <libwavy/log-macros.hpp>
#include <vector>

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
}

/**
 * @namespace libwavy::hls
 * @brief Contains HLS segmenter functionality for encoding and playlist generation.
 */
namespace libwavy::ffmpeg::hls
{

/**
 * @class HLS_Segmenter
 * @brief Handles segmentation of audio files into HLS streams and playlist generation.
 */
class WAVY_API HLS_Segmenter
{
private:
  std::vector<int> found_bitrates;

public:
  libwavy::ffmpeg::Metadata lbwMetadata;

  HLS_Segmenter();
  ~HLS_Segmenter();

  auto createSegmentsFLAC(const AbsPath& input_file, const Directory& output_dir,
                          CStrRelPath output_playlist, int bitrate) -> bool;

  auto createSegments(CStrRelPath input_file, CStrDirectory output_dir, bool use_flac = false)
    -> std::vector<int>;

  void createMasterPlaylistMP3(const Directory& input_dir, const Directory& output_dir);

private:
  auto encode_variant(CStrRelPath input_file, CStrRelPath output_playlist, int bitrate) -> bool;
};

} // namespace libwavy::ffmpeg::hls
