#pragma once

#include "toml.hpp"
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string_view>

using namespace std;
namespace toml_fs = std::filesystem;

/**
 * @brief Macros for parent and field names used in the TOML configuration.
 */

// File section
#define PARENT_FILE "File"
#define PARENT_FILE_FIELD_PATH "path"

// Format section
#define PARENT_FORMAT "Format"
#define PARENT_FORMAT_FIELD_TYPE "audio_type"
#define PARENT_FORMAT_FIELD_DURATION "duration"
#define PARENT_FORMAT_FIELD_BITRATE "parsed_bitrate"

// Metadata section
#define PARENT_METADATA "Metadata"
#define PARENT_METADATA_FIELD_TITLE "title"
#define PARENT_METADATA_FIELD_ARTIST "artist"
#define PARENT_METADATA_FIELD_TRACK "track"
#define PARENT_METADATA_FIELD_ALBUM "album"
#define PARENT_METADATA_FIELD_DISC "disc"
#define PARENT_METADATA_FIELD_COPYRIGHT "copyright"
#define PARENT_METADATA_FIELD_GENRE "genre"
#define PARENT_METADATA_FIELD_COMMENT "comment"
#define PARENT_METADATA_FIELD_ALBUM_ARTIST "album_artist"
#define PARENT_METADATA_FIELD_TSRC "TSRC"
#define PARENT_METADATA_FIELD_ENCODER "encoder"
#define PARENT_METADATA_FIELD_ENCODED_BY "encoded_by"
#define PARENT_METADATA_FIELD_DATE "date"

/**
 * @brief Checks if the metadata file exists.
 *
 * @param filePath The path to the metadata file.
 * @return `true` if the file exists, otherwise `false`.
 */
bool metadataFileExists(const string& filePath) { 
    return toml_fs::exists(filePath); 
}

/**
 * @brief Loads a metadata TOML file.
 *
 * @param filePath Path to the metadata TOML file
 * @return A toml::parse_result object representing the parsed metadata
 * @throws std::runtime_error If the file cannot be parsed
 */
auto loadMetadata(const string& filePath)
{
    if (!metadataFileExists(filePath))
    {
        cerr << "ERROR: Metadata file not found: " << filePath << endl;
        exit(EXIT_FAILURE);
    }

    return toml::parse_file(filePath);
}

/**
 * @brief Parses a string field from the metadata TOML configuration.
 *
 * @param metadata The parsed TOML metadata
 * @param parent The parent section name
 * @param field The field name within the parent section
 * @return A string view representing the value of the field
 */
auto parseMetadataField(const toml::parse_result& metadata, string_view parent, 
                       string_view field) -> string_view
{
    return metadata[parent][field].value_or(""sv);
}

/**
 * @brief Parses an integer field from the metadata TOML configuration.
 *
 * @param metadata The parsed TOML metadata
 * @param parent The parent section name
 * @param field The field name within the parent section
 * @return The integer value of the field, or -1 if not found
 */
auto parseMetadataFieldInt(const toml::parse_result& metadata, string_view parent, 
                          string_view field) -> int64_t
{
    return metadata[parent][field].value_or(-1);
}

/**
 * @brief Parses a fraction field (e.g., "6/12") from the metadata.
 *
 * @param value The string containing the fraction
 * @return A pair of integers representing numerator and denominator
 */
auto parseFraction(string_view value) -> pair<int, int>
{
    size_t pos = value.find('/');
    if (pos == string::npos) {
        return {stoi(string(value)), 0};
    }
    return {
        stoi(string(value.substr(0, pos))),
        stoi(string(value.substr(pos + 1)))
    };
}

/**
 * @brief Structure to hold parsed audio metadata
 */
struct AudioMetadata {
    string path;
    string audio_type;
    int64_t duration;
    string bitrate;
    string title;
    string artist;
    string album;
    pair<int, int> track;
    pair<int, int> disc;
    string copyright;
    string genre;
    string comment;
    string album_artist;
    string tsrc;
    string encoder;
    string encoded_by;
    string date;
};

/**
 * @brief Parses a metadata TOML file into an AudioMetadata structure
 *
 * @param filePath Path to the metadata TOML file
 * @return AudioMetadata structure containing the parsed data
 */
auto parseAudioMetadata(const string& filePath) -> AudioMetadata
{
    auto metadata = loadMetadata(filePath);
    AudioMetadata result;

    result.path = string(parseMetadataField(metadata, PARENT_FILE, PARENT_FILE_FIELD_PATH));
    result.audio_type = string(parseMetadataField(metadata, PARENT_FORMAT, PARENT_FORMAT_FIELD_TYPE));
    result.duration = parseMetadataFieldInt(metadata, PARENT_FORMAT, PARENT_FORMAT_FIELD_DURATION);
    result.bitrate = string(parseMetadataField(metadata, PARENT_FORMAT, PARENT_FORMAT_FIELD_BITRATE));
    
    result.title = string(parseMetadataField(metadata, PARENT_METADATA, PARENT_METADATA_FIELD_TITLE));
    result.artist = string(parseMetadataField(metadata, PARENT_METADATA, PARENT_METADATA_FIELD_ARTIST));
    result.album = string(parseMetadataField(metadata, PARENT_METADATA, PARENT_METADATA_FIELD_ALBUM));
    
    result.track = parseFraction(parseMetadataField(metadata, PARENT_METADATA, PARENT_METADATA_FIELD_TRACK));
    result.disc = parseFraction(parseMetadataField(metadata, PARENT_METADATA, PARENT_METADATA_FIELD_DISC));
    
    result.copyright = string(parseMetadataField(metadata, PARENT_METADATA, PARENT_METADATA_FIELD_COPYRIGHT));
    result.genre = string(parseMetadataField(metadata, PARENT_METADATA, PARENT_METADATA_FIELD_GENRE));
    result.comment = string(parseMetadataField(metadata, PARENT_METADATA, PARENT_METADATA_FIELD_COMMENT));
    result.album_artist = string(parseMetadataField(metadata, PARENT_METADATA, PARENT_METADATA_FIELD_ALBUM_ARTIST));
    result.tsrc = string(parseMetadataField(metadata, PARENT_METADATA, PARENT_METADATA_FIELD_TSRC));
    result.encoder = string(parseMetadataField(metadata, PARENT_METADATA, PARENT_METADATA_FIELD_ENCODER));
    result.encoded_by = string(parseMetadataField(metadata, PARENT_METADATA, PARENT_METADATA_FIELD_ENCODED_BY));
    result.date = string(parseMetadataField(metadata, PARENT_METADATA, PARENT_METADATA_FIELD_DATE));

    return result;
}

/**
 * @brief Parses a metadata TOML file into an AudioMetadata structure
 *
 * @param dataString String containing file data
 * @return AudioMetadata structure containing the parsed data
 */
auto parseAudioMetadataFromDataString(const string& dataString) -> AudioMetadata
{
    auto metadata = toml::parse(dataString);
    AudioMetadata result;

    result.path = string(parseMetadataField(metadata, PARENT_FILE, PARENT_FILE_FIELD_PATH));
    result.audio_type = string(parseMetadataField(metadata, PARENT_FORMAT, PARENT_FORMAT_FIELD_TYPE));
    result.duration = parseMetadataFieldInt(metadata, PARENT_FORMAT, PARENT_FORMAT_FIELD_DURATION);
    result.bitrate = string(parseMetadataField(metadata, PARENT_FORMAT, PARENT_FORMAT_FIELD_BITRATE));
    
    result.title = string(parseMetadataField(metadata, PARENT_METADATA, PARENT_METADATA_FIELD_TITLE));
    result.artist = string(parseMetadataField(metadata, PARENT_METADATA, PARENT_METADATA_FIELD_ARTIST));
    result.album = string(parseMetadataField(metadata, PARENT_METADATA, PARENT_METADATA_FIELD_ALBUM));
    
    result.track = parseFraction(parseMetadataField(metadata, PARENT_METADATA, PARENT_METADATA_FIELD_TRACK));
    result.disc = parseFraction(parseMetadataField(metadata, PARENT_METADATA, PARENT_METADATA_FIELD_DISC));
    
    result.copyright = string(parseMetadataField(metadata, PARENT_METADATA, PARENT_METADATA_FIELD_COPYRIGHT));
    result.genre = string(parseMetadataField(metadata, PARENT_METADATA, PARENT_METADATA_FIELD_GENRE));
    result.comment = string(parseMetadataField(metadata, PARENT_METADATA, PARENT_METADATA_FIELD_COMMENT));
    result.album_artist = string(parseMetadataField(metadata, PARENT_METADATA, PARENT_METADATA_FIELD_ALBUM_ARTIST));
    result.tsrc = string(parseMetadataField(metadata, PARENT_METADATA, PARENT_METADATA_FIELD_TSRC));
    result.encoder = string(parseMetadataField(metadata, PARENT_METADATA, PARENT_METADATA_FIELD_ENCODER));
    result.encoded_by = string(parseMetadataField(metadata, PARENT_METADATA, PARENT_METADATA_FIELD_ENCODED_BY));
    result.date = string(parseMetadataField(metadata, PARENT_METADATA, PARENT_METADATA_FIELD_DATE));

    return result;
}
