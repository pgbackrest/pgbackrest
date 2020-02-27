/***********************************************************************************************************************************
Compression Helper
***********************************************************************************************************************************/
#ifndef COMMON_COMPRESS_HELPER_H
#define COMMON_COMPRESS_HELPER_H

#include <stdbool.h>

/***********************************************************************************************************************************
Available compression types
***********************************************************************************************************************************/
typedef enum
{
    compressTypeNone,                                               // No compression
    compressTypeGz,                                                 // gzip
    compressTypeLz4,                                                // lz4

    // These types have not been implemented but are included here so older versions can identify compression types added by future
    // versions. In that sense this list is speculative, but these seem to be all the types that are likely to be added in the
    // foreseeable future.
    compressTypeZst,                                                // zstandard
    compressTypeXz,                                                 // xz/lzma
    compressTypeBz2,                                                // bzip2
} CompressType;

#include <common/type/string.h>
#include <common/io/filter/group.h>

/***********************************************************************************************************************************
Compression types as a regexp. Ideally this would be generated automatically at build time from the known compression types but
there shouldn't be a lot of churn, so just define it statically.
***********************************************************************************************************************************/
#define COMPRESS_TYPE_REGEXP                                        "(\\.gz|\\.lz4|\\.zst|\\.xz|\\.bz2)"

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Get enum from a compression type string
CompressType compressTypeEnum(const String *type);

// Check that a valid compress type is compiled into this binary.  Errors when the compress type is not present.
void compressTypePresent(CompressType type);

// Get string representation of a compression type. This is the the extension without the period.
const String *compressTypeStr(CompressType type);

// Get compression type from a (file) name by checking the extension. If the extension is not a supported compression type then
// compressType none is returned, even if the file is compressed with some unknown type.
CompressType compressTypeFromName(const String *name);

// Compression filter for the specified type.  Error when compress type is none or invalid.
IoFilter *compressFilter(CompressType type, int level);

// Compression/decompression filter based on string type and a parameter list.  This is useful when a filter must be created on a
// remote system since the filter type and parameters can be passed through a protocol.
IoFilter *compressFilterVar(const String *filterType, const VariantList *filterParamList);

// Decompression filter for the specified type.  Error when compress type is none or invalid.
IoFilter *decompressFilter(CompressType type);

// Get extension for the current compression type
const String *compressExtStr(CompressType type);

// Add extension for current compression type to the file
void compressExtCat(String *file, CompressType type);

// Remove the specified compression extension. Error when the extension is not correct.
String *compressExtStrip(const String *file, CompressType type);

/***********************************************************************************************************************************
Internal Functions
***********************************************************************************************************************************/
// Default compression level for a compression type, used while loading the configuration
int compressLevelDefault(CompressType type);

#endif
