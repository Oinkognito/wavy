#ifndef COMPRESSION_REPACKING_HEADER_H
#define COMPRESSION_REPACKING_HEADER_H

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

#include "common.h" // Helper functions, CHECK(), and CHECK_ZSTD()
#include "zstd-logging.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zstd.h> // presumes zstd library is installed
#include <archive.h>
#include <archive_entry.h>
#include <dirent.h>
#include <libgen.h>

/* ─────────────────────── RESOURCE STRUCTURE ─────────────────────── */
typedef struct
{
  void*      fBuffer;
  void*      cBuffer;
  size_t     fBufferSize;
  size_t     cBufferSize;
  ZSTD_CCtx* cctx;
} resources;

/* ─────────────────────── RESOURCE ALLOCATION ─────────────────────── */
static resources createResources_orDie(int argc, const char** argv, char** ofn,
                                       size_t* ofnBufferLen)
{
  ZSTD_LOG_START_SECTION("Initializing Resources");

  size_t maxFilenameLength = 0;
  size_t maxFileSize       = 0;

  for (int argNb = 1; argNb < argc; argNb++)
  {
    const char* const filename       = argv[argNb];
    size_t const      filenameLength = strlen(filename);
    size_t const      fileSize       = fsize_orDie(filename);

    if (filenameLength > maxFilenameLength)
      maxFilenameLength = filenameLength;
    if (fileSize > maxFileSize)
      maxFileSize = fileSize;
  }

  resources ress;
  ress.fBufferSize = maxFileSize;
  ress.cBufferSize = ZSTD_compressBound(maxFileSize);

  *ofnBufferLen = maxFilenameLength + 5;
  *ofn          = (char*)malloc_orDie(*ofnBufferLen);
  ress.fBuffer  = malloc_orDie(ress.fBufferSize);
  ress.cBuffer  = malloc_orDie(ress.cBufferSize);
  ress.cctx     = ZSTD_createCCtx();

  CHECK(ress.cctx != NULL, "ZSTD_createCCtx() failed!");

  ZSTD_LOG_SUCCESS("Resources allocated successfully.");
  ZSTD_LOG_END_SECTION();

  return ress;
}

static resources createResourcesForFile(const char* filename, char** ofn, size_t* ofnBufferLen)
{
    ZSTD_LOG_START_SECTION("Initializing Resources for Single File");

    // Get the length of the filename and the file size
    size_t const filenameLength = strlen(filename);
    size_t const fileSize       = fsize_orDie(filename);  // Assuming fsize_orDie is defined elsewhere

    // Allocate memory for the filename buffer
    *ofnBufferLen = filenameLength + 5;  // +5 for the .zst extension or other additional space
    *ofn = (char*)malloc_orDie(*ofnBufferLen);

    // Copy the filename into the allocated buffer
    strncpy(*ofn, filename, filenameLength);
    (*ofn)[filenameLength] = '\0';  // Null-terminate the string

    // Allocate resources for the buffers and compression context
    resources ress;
    ress.fBufferSize = fileSize;
    ress.cBufferSize = ZSTD_compressBound(fileSize);
    
    ress.fBuffer = malloc_orDie(ress.fBufferSize);  // File buffer
    ress.cBuffer = malloc_orDie(ress.cBufferSize);  // Compressed buffer
    ress.cctx = ZSTD_createCCtx();  // Compression context

    CHECK(ress.cctx != NULL, "ZSTD_createCCtx() failed!");

    ZSTD_LOG_SUCCESS("Resources for single file allocated successfully.");

    return ress;
}

/* ─────────────────────── RESOURCE DEALLOCATION ─────────────────────── */
static void freeResources(resources ress, char* outFilename,const char* outputDir)
{
  ZSTD_LOG_END_SECTION();
  ZSTD_LOG_START_SECTION("Releasing Resources");

  free(ress.fBuffer);
  free(ress.cBuffer);
  ZSTD_freeCCtx(ress.cctx);
  free(outFilename);

  ZSTD_LOG_SUCCESS("All resources released successfully.");
  ZSTD_LOG_END_SECTION();
}

/* ─────────────────────── ZSTD FILE COMPRESSION ─────────────────────── */
static void compressFile_orDie(resources ress, const char* fname, const char* oname,
                               size_t* totalOriginalSize, size_t* totalCompressedSize)
{
  ZSTD_LOG_INFO("Compressing file: %s", fname);

  size_t fSize = loadFile_orDie(fname, ress.fBuffer, ress.fBufferSize);

  size_t const cSize =
    ZSTD_compressCCtx(ress.cctx, ress.cBuffer, ress.cBufferSize, ress.fBuffer, fSize, 1);
  CHECK_ZSTD(cSize);

  saveFile_orDie(oname, ress.cBuffer, cSize);

  double reduction = (1.0 - ((double)cSize / (double)fSize)) * 100.0;

  *totalOriginalSize += fSize;
  *totalCompressedSize += cSize;

  ZSTD_LOG_SUCCESS("%s : %u -> %u (%5.2f%% smaller) - %s",
                   fname, (unsigned)fSize, (unsigned)cSize, reduction, oname);
}

/* ─────────────────────── FINAL STATISTICS ─────────────────────── */
static void printTotalSizeComparison(size_t totalOriginalSize, size_t totalCompressedSize)
{
  double reduction = (1.0 - ((double)totalCompressedSize / (double)totalOriginalSize)) * 100.0;

  printf("--------------- ZSTD Total Compression Statistics ---------------\n");
  printf("> Original size: %u bytes\n", (unsigned)totalOriginalSize);
  printf("> Compressed size: %u bytes\n", (unsigned)totalCompressedSize);
  printf("> Total reduction: %.2f%%\n", reduction);
  printf("--------------- ZSTD Total Compression Statistics ---------------\n");
}

/* ─────────────────────── FILE SIZE CALCULATION ─────────────────────── */
size_t get_file_size(const char *filename)
{
    struct stat st;
    if (stat(filename, &st) != 0) {
        return 0;
    }
    return st.st_size;
}

/*
 * After compression of all playlist and transport streams using zstd algorithm,
 * 
 * All zstd files must be sent over to the server in one go so we compress them into one tar file.
 *
 * >> The compression into tar logic is written in C++ <<
 *
 */

/* ─────────────────────── TAR COMPRESSION OF ZSTD FILES (SAMPLE) ─────────────────────── */
int create_tar_from_directory(const char *dirPath, const char *tarFilePath)
{
    struct archive *tar;
    struct archive_entry *entry;
    DIR *dir;
    struct dirent *entryFile;
    char filePath[1024];

    // Open the tar file for writing
    tar = archive_write_new();
    archive_write_set_format_pax_restricted(tar);  // Set tar format
    archive_write_open_filename(tar, tarFilePath);

    // Open the directory
    dir = opendir(dirPath);
    if (dir == NULL) {
        perror("Failed to open directory");
        return -1;
    }

    // Iterate over the files in the directory
    while ((entryFile = readdir(dir)) != NULL) {
        // Skip . and ..
        if (strcmp(entryFile->d_name, ".") == 0 || strcmp(entryFile->d_name, "..") == 0) {
            continue;
        }

        snprintf(filePath, sizeof(filePath), "%s/%s", dirPath, entryFile->d_name);

        // Create a new archive entry for the file
        entry = archive_entry_new();
        archive_entry_set_pathname(entry, entryFile->d_name);
        archive_entry_set_size(entry, get_file_size(filePath));
        archive_entry_set_filetype(entry, AE_IFREG);
        archive_entry_set_perm(entry, 0644);

        // Add the entry to the tar
        archive_write_header(tar, entry);

        // Open the file and write it to the tar archive
        FILE *file = fopen(filePath, "rb");
        if (file == NULL) {
            perror("Failed to open file for reading");
            return -1;
        }

        char buffer[1024];
        size_t bytesRead;
        while ((bytesRead = fread(buffer, 1, sizeof(buffer), file)) > 0) {
            archive_write_data(tar, buffer, bytesRead);
        }

        fclose(file);
        archive_entry_free(entry);
    }

    closedir(dir);
    archive_write_close(tar);
    archive_write_free(tar);

    return 0;
}

bool ZSTD_compressFilesInDirectory(const char* directory, const char* outputDir)
{
    DIR* dir = opendir(directory);
    if (!dir) {
        perror("-- [ZSTD] Error opening directory");
        return false;
    }

    struct dirent* entry;
    size_t totalOriginalSize = 0;
    size_t totalCompressedSize = 0;

    struct stat st = {0};
    if (stat(outputDir, &st) == -1) {
        mkdir(outputDir, 0700);
    }

    while ((entry = readdir(dir)) != NULL)
    {
        if (entry->d_type != DT_REG || entry->d_name[0] == '.') {
            continue;
        }

        // Dynamically allocate memory for the input file path
        size_t inputFileLen = strlen(directory) + strlen(entry->d_name) + 2;  // +1 for '/', +1 for '\0'
        char* inputFile = (char*)malloc(inputFileLen);
        if (!inputFile) {
            perror("-- [ZSTD] Memory allocation failed for input file path");
            closedir(dir);
            return false;
        }

        snprintf(inputFile, inputFileLen, "%s/%s", directory, entry->d_name);

        char* outFilename;
        size_t outFilenameLen;
        resources ress = createResourcesForFile(inputFile, &outFilename, &outFilenameLen);

        char* outputFile = (char*)malloc(outFilenameLen);
        if (!outputFile) {
            perror("-- [ZSTD] Memory allocation failed for output file path");
            free(inputFile);
            closedir(dir);
            return false;
        }

        snprintf(outputFile, outFilenameLen, "%s.zst", inputFile);

        compressFile_orDie(ress, inputFile, outputFile, &totalOriginalSize, &totalCompressedSize);

        size_t destFileLen = strlen(outputDir) + strlen(basename(outputFile)) + 2;  // +1 for '/', +1 for '\0'
        char* destFile = (char*)malloc(destFileLen);
        if (!destFile) {
            perror("-- [ZSTD] Memory allocation failed for destination file path");
            free(inputFile);
            free(outputFile);
            closedir(dir);
            return false;
        }

        snprintf(destFile, destFileLen, "%s/%s", outputDir, basename(outputFile));

        if (rename(outputFile, destFile) != 0) {
            perror("-- [ZSTD] Error moving file");
            free(inputFile);
            free(outputFile);
            free(destFile);
            closedir(dir);
            exit(1);
        }

        freeResources(ress, outFilename, NULL);
        free(inputFile);
        free(outputFile);
        free(destFile);
    }

    printTotalSizeComparison(totalOriginalSize, totalCompressedSize);

    closedir(dir);  // Close the directory
    
    return true;
}

#ifdef __cplusplus
}
#endif

#endif // ZSTD_COMPRESSION_HEADER_H
