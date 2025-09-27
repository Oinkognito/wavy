#pragma once

#include <filesystem>
#include <fstream>
#include <libwavy/common/macros.hpp>
#include <libwavy/common/types.hpp>
#include <libwavy/log-macros.hpp>
#include <openssl/evp.h>
#include <optional>

namespace fs = std::filesystem;

namespace libwavy::server::auth
{

static auto compute_sha256_hex(const std::string& file_path) -> std::optional<std::string>
{
  EVP_MD_CTX* mdctx = EVP_MD_CTX_new();
  if (!mdctx)
    return std::nullopt;

  const EVP_MD* md = EVP_sha256();
  if (EVP_DigestInit_ex(mdctx, md, nullptr) != 1)
  {
    EVP_MD_CTX_free(mdctx);
    return std::nullopt;
  }

  std::ifstream file(file_path, std::ios::binary);
  if (!file)
  {
    EVP_MD_CTX_free(mdctx);
    return std::nullopt;
  }

  std::vector<unsigned char> buffer(8192);
  while (file)
  {
    file.read(reinterpret_cast<char*>(buffer.data()), buffer.size());
    std::streamsize read_bytes = file.gcount();
    if (read_bytes > 0)
    {
      if (EVP_DigestUpdate(mdctx, buffer.data(), static_cast<size_t>(read_bytes)) != 1)
      {
        EVP_MD_CTX_free(mdctx);
        return std::nullopt;
      }
    }
  }

  unsigned char digest[EVP_MAX_MD_SIZE];
  unsigned int  digest_len = 0;
  if (EVP_DigestFinal_ex(mdctx, digest, &digest_len) != 1)
  {
    EVP_MD_CTX_free(mdctx);
    return std::nullopt;
  }

  EVP_MD_CTX_free(mdctx);

  std::ostringstream oss;
  for (unsigned int i = 0; i < digest_len; ++i)
  {
    oss << std::hex << std::setw(2) << std::setfill('0') << (int)digest[i];
  }
  return oss.str();
}

// helper: persist key to keystore
static auto persist_key(const StorageAudioID& audio_id, const std::string& key) -> bool
{
  try
  {
    log::TRACE<log::SERVER_UPLD>("Found SHA256 key ({}) for Audio ID: {}", key, audio_id);

    const fs::path keys_dir = fs::path(macros::to_string(macros::SERVER_STORAGE_DIR_KEYS));
    fs::create_directories(keys_dir);
    fs::path key_file = keys_dir / (audio_id + ".key");

    // Atomic write: write to temp and rename
    fs::path tmp = key_file;
    tmp += ".tmp";
    std::ofstream ofs(tmp, std::ios::binary | std::ios::trunc);
    if (!ofs)
      return false;
    ofs << key;
    ofs.close();
    fs::rename(tmp, key_file);
    return true;
  }
  catch (...)
  {
    return false;
  }
}

} // namespace libwavy::server::auth
