#include <dirent.h> // For working with directories
#include <libwavy/zstd/compression.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/*
 * Does not work well with already compressed files like .tar!!
 *
 * In terms of transport streams and playlist files (.ts & .m3u8),
 * a compression schema of the following works decently well (not as well as
 * hoped):
 *
 * [.ts / .m3u8]
 *     |
 *     |
 *     |
 *     V
 * ZSTD compression (each file)
 *     ||
 *     ||
 *     ||
 *     \/
 * [.ts.zst / .m3u8.zst]
 *     ||
 *     ||
 *     ||
 *     \/
 * TAR compression (all files)
 *     |
 *     |
 *     |
 *     V
 * hls_data.tar.gz
 *
 * @IMPORTANT
 *
 * The TAR compression is to just bundle all zst files into one, there is really
 * no more compression that is possible.
 *
 * @OBSERVATION:
 *
 * $SAMPLE DATA: MP3 file of size 3.7MiB of duration 4:14
 *
 * Simple tar compression with libarchive ==> 11.1MiB
 * Zstd compression + overall tar compression ==> 10.8MiB
 *
 */

int main(int argc, const char **argv) {
  if (argc != 3) {
    ZSTD_LOG_ERROR("Usage: %s <directory> <output_directory>\n", argv[0]);
    return 1;
  }

  const char *directory = argv[1];
  const char *outputDir = argv[2];

  ZSTD_compressFilesInDirectory(directory, outputDir); // Call the function
  create_tar_from_directory(outputDir, "."); // to generate a single tar file

  return 0;
}
