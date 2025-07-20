#pragma once

#include <libwavy/common/types.hpp>
#include <vector>

/* PROTOTYPES DEFINITION FOR VALIDATION IN SERVER */
auto is_valid_extension(const AbsPath& filename) -> bool;
auto validate_m3u8_format(const PlaylistData& content) -> bool;
auto validate_ts_file(const std::vector<ui8>& data) -> bool;
auto validate_m4s(const AbsPath& m4s_path) -> bool;
auto extract_and_validate(const AbsPath& gzip_path, const StorageAudioID& audio_id) -> bool;
void removeBodyPadding(std::string& body);
auto tokenizePath(std::istringstream& iss) -> std::vector<std::string>;
