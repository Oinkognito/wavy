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

/*
 * This header file has common utility functions used in examples.
 *
 * This is header from Facebook's ZSTD repository with some modifications
 */
#ifndef COMMON_H
#define COMMON_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <errno.h>    // errno
#include <stdio.h>    // fprintf, perror, fopen, etc.
#include <stdlib.h>   // malloc, free, exit
#include <string.h>   // strerror
#include <sys/stat.h> // stat
#include <zstd.h>

/* UNUSED_ATTR tells the compiler it is okay if the function is unused. */
#if defined(__GNUC__)
#define UNUSED_ATTR __attribute__((unused))
#else
#define UNUSED_ATTR
#endif

#define HEADER_FUNCTION static UNUSED_ATTR

  constexpr double bytes_to_mib(std::size_t bytes)
  {
    return static_cast<double>(bytes) / (1024 * 1024);
  }

  /*
   * Define the returned error code from utility functions.
   */
  typedef enum
  {
    ERROR_fsize     = 1,
    ERROR_fopen     = 2,
    ERROR_fclose    = 3,
    ERROR_fread     = 4,
    ERROR_fwrite    = 5,
    ERROR_loadFile  = 6,
    ERROR_saveFile  = 7,
    ERROR_malloc    = 8,
    ERROR_largeFile = 9,
  } COMMON_ErrorCode;

/*! CHECK
 * Check that the condition holds. If it doesn't print a message and die.
 */
#define CHECK(cond, ...)                                                      \
  do                                                                          \
  {                                                                           \
    if (!(cond))                                                              \
    {                                                                         \
      fprintf(stderr, "%s:%d CHECK(%s) failed: ", __FILE__, __LINE__, #cond); \
      fprintf(stderr, "" __VA_ARGS__);                                        \
      fprintf(stderr, "\n");                                                  \
      exit(1);                                                                \
    }                                                                         \
  } while (0)

/*! CHECK_ZSTD
 * Check the zstd error code and die if an error occurred after printing a
 * message.
 */
#define CHECK_ZSTD(fn)                                       \
  do                                                         \
  {                                                          \
    size_t const err = (fn);                                 \
    CHECK(!ZSTD_isError(err), "%s", ZSTD_getErrorName(err)); \
  } while (0)

  /*! fsize_orDie() :
   * Get the size of a given file path.
   *
   * @return The size of a given file path.
   */
  HEADER_FUNCTION size_t fsize_orDie(const char* filename)
  {
    struct stat st;
    if (stat(filename, &st) != 0)
    {
      /* error */
      perror(filename);
      exit(ERROR_fsize);
    }

    off_t const  fileSize = st.st_size;
    size_t const size     = (size_t)fileSize;
    /* 1. fileSize should be non-negative,
     * 2. if off_t -> size_t type conversion results in discrepancy,
     *    the file size is too large for type size_t.
     */
    if ((fileSize < 0) || (fileSize != (off_t)size))
    {
      fprintf(stderr, "%s : filesize too large \n", filename);
      exit(ERROR_largeFile);
    }
    return size;
  }

  /*! fopen_orDie() :
   * Open a file using given file path and open option.
   *
   * @return If successful this function will return a FILE pointer to an
   * opened file otherwise it sends an error to stderr and exits.
   */
  HEADER_FUNCTION FILE* fopen_orDie(const char* filename, const char* instruction)
  {
    FILE* const inFile = fopen(filename, instruction);
    if (inFile)
      return inFile;
    /* error */
    perror(filename);
    exit(ERROR_fopen);
  }

  /*! fclose_orDie() :
   * Close an opened file using given FILE pointer.
   */
  HEADER_FUNCTION void fclose_orDie(FILE* file)
  {
    if (!fclose(file))
    {
      return;
    };
    /* error */
    perror("fclose");
    exit(ERROR_fclose);
  }

  /*! fread_orDie() :
   *
   * Read sizeToRead bytes from a given file, storing them at the
   * location given by buffer.
   *
   * @return The number of bytes read.
   */
  HEADER_FUNCTION size_t fread_orDie(void* buffer, size_t sizeToRead, FILE* file)
  {
    size_t const readSize = fread(buffer, 1, sizeToRead, file);
    if (readSize == sizeToRead)
      return readSize; /* good */
    if (feof(file))
      return readSize; /* good, reached end of file */
    /* error */
    perror("fread");
    exit(ERROR_fread);
  }

  /*! fwrite_orDie() :
   *
   * Write sizeToWrite bytes to a file pointed to by file, obtaining
   * them from a location given by buffer.
   *
   * Note: This function will send an error to stderr and exit if it
   * cannot write data to the given file pointer.
   *
   * @return The number of bytes written.
   */
  HEADER_FUNCTION size_t fwrite_orDie(const void* buffer, size_t sizeToWrite, FILE* file)
  {
    size_t const writtenSize = fwrite(buffer, 1, sizeToWrite, file);
    if (writtenSize == sizeToWrite)
      return sizeToWrite; /* good */
    /* error */
    perror("fwrite");
    exit(ERROR_fwrite);
  }

  /*! malloc_orDie() :
   * Allocate memory.
   *
   * @return If successful this function returns a pointer to allo-
   * cated memory.  If there is an error, this function will send that
   * error to stderr and exit.
   */
  HEADER_FUNCTION void* malloc_orDie(size_t size)
  {
    void* const buff = malloc(size);
    if (buff)
      return buff;
    /* error */
    perror("malloc");
    exit(ERROR_malloc);
  }

  /*! loadFile_orDie() :
   * load file into buffer (memory).
   *
   * Note: This function will send an error to stderr and exit if it
   * cannot read data from the given file path.
   *
   * @return If successful this function will load file into buffer and
   * return file size, otherwise it will printout an error to stderr and exit.
   */
  HEADER_FUNCTION size_t loadFile_orDie(const char* fileName, void* buffer, size_t bufferSize)
  {
    size_t const fileSize = fsize_orDie(fileName);
    CHECK(fileSize <= bufferSize, "File too large!");

    FILE* const  inFile   = fopen_orDie(fileName, "rb");
    size_t const readSize = fread(buffer, 1, fileSize, inFile);
    if (readSize != (size_t)fileSize)
    {
      fprintf(stderr, "fread: %s : %s \n", fileName, strerror(errno));
      exit(ERROR_fread);
    }
    fclose(inFile); /* can't fail, read only */
    return fileSize;
  }

  /*! mallocAndLoadFile_orDie() :
   * allocate memory buffer and then load file into it.
   *
   * Note: This function will send an error to stderr and exit if memory allocation
   * fails or it cannot read data from the given file path.
   *
   * @return If successful this function will return buffer and bufferSize(=fileSize),
   * otherwise it will printout an error to stderr and exit.
   */
  HEADER_FUNCTION void* mallocAndLoadFile_orDie(const char* fileName, size_t* bufferSize)
  {
    size_t const fileSize = fsize_orDie(fileName);
    *bufferSize           = fileSize;
    void* const buffer    = malloc_orDie(*bufferSize);
    loadFile_orDie(fileName, buffer, *bufferSize);
    return buffer;
  }

  /*! saveFile_orDie() :
   *
   * Save buffSize bytes to a given file path, obtaining them from a location pointed
   * to by buff.
   *
   * Note: This function will send an error to stderr and exit if it
   * cannot write to a given file.
   */
  HEADER_FUNCTION void saveFile_orDie(const char* fileName, const void* buff, size_t buffSize)
  {
    FILE* const  oFile = fopen_orDie(fileName, "wb");
    size_t const wSize = fwrite(buff, 1, buffSize, oFile);
    if (wSize != (size_t)buffSize)
    {
      fprintf(stderr, "fwrite: %s : %s \n", fileName, strerror(errno));
      exit(ERROR_fwrite);
    }
    if (fclose(oFile))
    {
      perror(fileName);
      exit(ERROR_fclose);
    }
  }

#ifdef __cplusplus
}
#endif

#endif
