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

#include <libwavy/common/state.hpp>
#include <libwavy/common/types.hpp>
#include <libwavy/db/entry.hpp>
#include <vector>

namespace libwavy::server::helpers
{

/* PROTOTYPES DEFINITION FOR VALIDATION IN SERVER */
auto is_valid_extension(const AbsPath& filename) -> bool;
auto validate_m3u8_format(const PlaylistData& content) -> bool;
auto validate_ts_file(const std::vector<ui8>& data) -> bool;
auto validate_m4s(const AbsPath& m4s_path) -> bool;
auto extract_and_validate(const RelPath& gzip_path, const StorageAudioID& audio_id,
                          db::LMDBKV<AudioMetadataPlain>& kv) -> StorageOwnerID;

} // namespace libwavy::server::helpers
