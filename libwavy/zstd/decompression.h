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

#ifndef ZSTD_DECOMP_H
#define ZSTD_DECOMP_H

/*
 * @NOTE
 *
 * This file is a modified version of Facebook's ZSTD lossless compression
 * algorithm API use to compress multiple files.
 *
 */

#ifdef __cplusplus
extern "C"
{
#endif

#include <libwavy/common/common.h> // Helper functions, CHECK(), and CHECK_ZSTD()
#include <stdio.h>                 // printf, fopen, fwrite
#include <stdlib.h>                // free, malloc
#include <zstd.h>                  // presumes zstd library is installed

  static bool ZSTD_decompress_file(const char* fname)
  {
    size_t      cSize;
    void* const cBuff = mallocAndLoadFile_orDie(fname, &cSize);

    // Read the content size from the frame header
    unsigned long long const rSize = ZSTD_getFrameContentSize(cBuff, cSize);
    if (rSize == ZSTD_CONTENTSIZE_ERROR)
    {
      fprintf(stderr, "%s: not compressed by zstd!\n", fname);
      free(cBuff);
      return false;
    }
    if (rSize == ZSTD_CONTENTSIZE_UNKNOWN)
    {
      fprintf(stderr, "%s: original size unknown!\n", fname);
      free(cBuff);
      return false;
    }

    void* const rBuff = malloc(rSize);
    if (!rBuff)
    {
      fprintf(stderr, "Failed to allocate memory for decompression buffer\n");
      free(cBuff);
      return false;
    }

    // Perform decompression
    size_t const dSize = ZSTD_decompress(rBuff, rSize, cBuff, cSize);
    if (ZSTD_isError(dSize))
    {
      fprintf(stderr, "Decompression failed: %s\n", ZSTD_getErrorName(dSize));
      free(rBuff);
      free(cBuff);
      return false;
    }

    // Check that the decompressed size matches the expected size
    if (dSize != rSize)
    {
      fprintf(stderr, "Decompression size mismatch\n");
      free(rBuff);
      free(cBuff);
      return false;
    }

    // Create output filename
    char outputFilename[256];
    snprintf(outputFilename, sizeof(outputFilename), "%.*s", (int)(strlen(fname) - 4), fname);

    // Open output file for writing
    FILE* outFile = fopen(outputFilename, "wb");
    if (!outFile)
    {
      fprintf(stderr, "Failed to open file for writing: %s\n", outputFilename);
      free(rBuff);
      free(cBuff);
      return false;
    }

    // Write the decompressed data to the output file
    size_t written = fwrite(rBuff, 1, dSize, outFile);
    if (written != dSize)
    {
      fprintf(stderr, "Failed to write all decompressed data\n");
      fclose(outFile);
      free(rBuff);
      free(cBuff);
      return false;
    }

    fclose(outFile);
    /*printf("Successfully decompressed: %s -> %s\n", fname, outputFilename);*/

    // Clean up and free memory
    free(rBuff);
    free(cBuff);

    return true; // Return true if everything was successful
  }

#ifdef __cplusplus
}
#endif

#endif
