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

#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

// As these are CRITICAL I/O operations, a runtime error is optimal imo
//
// Using libwavy::log is not that beneficial as it introduces more compilation time
// and unnecessary boilerplate to this.
//
// Utils are supposed to be FAST and SIMPLE. You can add the extensive libwavy::log
// on top of libwavy::util (with a try catch block its pretty easy.)
//
// This FileUtil template should be able to handle std::string and boost::filesystem::path

namespace fs = std::filesystem;

namespace libwavy::utils
{

/**
 * Rename a file like `mv`:
 * - If src and dst are on the same filesystem, uses atomic rename().
 * - If on different filesystems (EBS ↔ S3, etc.), falls back to copy + delete.
 * - Ensures permissions and existence checks are handled safely.
 */
inline void rename_with_fallback(const fs::path& src, const fs::path& dst)
{
  std::error_code ec;

  if (!fs::exists(src))
  {
    throw std::filesystem::filesystem_error("Source does not exist", src, ec);
  }

  // Try fast path (atomic rename)
  fs::rename(src, dst, ec);
  if (!ec)
    return; // success

  // If rename failed due to cross-device link (EXDEV), do copy+remove
  if (ec.value() == EXDEV)
  {
    // Make sure parent directory exists
    fs::create_directories(dst.parent_path(), ec);
    if (ec)
      throw std::filesystem::filesystem_error("Failed to create destination dir", dst.parent_path(),
                                              ec);

    // Copy file contents
    fs::copy_file(src, dst, fs::copy_options::overwrite_existing, ec);
    if (ec)
      throw std::filesystem::filesystem_error("Copy failed", src, dst, ec);

    // Try to preserve timestamps & permissions (optional)
    std::error_code tmp;
    fs::permissions(dst, fs::status(src).permissions(), tmp);

    // Remove original
    fs::remove(src, ec);
    if (ec)
      throw std::filesystem::filesystem_error("Remove failed", src, ec);
    return;
  }

  // If failed for another reason, surface it
  throw std::filesystem::filesystem_error("rename_with_fallback failed", src, dst, ec);
}

template <typename PathType> class FileUtil
{
public:
  static auto readFile(const PathType& path) -> std::string
  {
    std::ifstream in(pathToString(path));
    if (!in)
      throw std::runtime_error("Unable to open file: " + pathToString(path));

    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
  }

  static auto fileExists(const PathType& path) -> bool
  {
    std::ifstream in(pathToString(path));
    return in.good();
  }

  static auto fileSize(const PathType& path) -> std::size_t
  {
    std::ifstream in(pathToString(path), std::ios::binary | std::ios::ate);
    if (!in)
      throw std::runtime_error("Unable to open file: " + pathToString(path));
    return in.tellg();
  }

  static void writeFile(const PathType& path, const std::string& content)
  {
    std::ofstream out(pathToString(path));
    if (!out)
      throw std::runtime_error("Unable to write to file: " + pathToString(path));
    out << content;
  }

private:
  static auto pathToString(const std::string& path) -> std::string { return path; }

#ifdef BOOST_FILESYSTEM_HPP
  static auto pathToString(const boost::filesystem::path& path) -> std::string
  {
    return path.string();
  }
#endif
};

} // namespace libwavy::utils
