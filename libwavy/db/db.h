#pragma once

#include "entry.hpp"
#include <libwavy/common/types.hpp>

using OwnerAudioIDMap = libwavy::MiniDB<StorageOwnerID, StorageAudioID>;

#define CREATE_WAVY_MINI_DB()              \
  namespace storage                        \
  {                                        \
  inline OwnerAudioIDMap g_owner_audio_db; \
  }
