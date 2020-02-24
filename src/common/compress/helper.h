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
    compressTypeGzip,                                               // gzip
    compressTypeLz4,                                                // lz4

    // These types have not been implemented but are included here so older versions can identify compression types added by future
    // versions. In that sense this list is speculative, but these seem to be all the types that are likely to be added.
    compressTypeZst,                                                // zstandard
    compressTypeXz,                                                 // xz/lzma
    compressTypeBz2,                                                // bzip2
} CompressType;

#include <common/type/string.h>
#include <common/io/filter/group.h>

/***********************************************************************************************************************************
Compression types as a regexp. Ideally this would be generated automatically at build time from the known compression types but
there is not likely to be a lot of churn, so just define it statically.
***********************************************************************************************************************************/
#define COMPRESS_TYPE_REGEXP                                        "(\\.gz|\\.lz4|\\.zst|\\.xz|\\.bz2)"

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Get enum from a compression type string
CompressType compressTypeEnum(const String *type);

// Get string representation of a compression type. This should be the extension.
const char *compressTypeZ(CompressType type);

// Get compression type from a (file) name by checking the extension. If the extension is not a supported compression type then
// compressType none is returned, even if the file is compressed with some unknown type.
CompressType compressTypeFromName(const String *name);

// Compression filter for the specified type.  If compress type is none then NULL is returned.
IoFilter *compressFilter(CompressType type, int level);

// Compression/decompression filter based on string type and a parameter list
IoFilter *compressFilterVar(const String *filterType, const VariantList *filterParamList);

// Decompression filter for the specified type.  If compress type is none then NULL is returned.
IoFilter *decompressFilter(CompressType type);

// Get extension for the current compression type
const char *compressExtZ(CompressType type);

// Add extension for current compression type to the file
void compressExtCat(String *file, CompressType type);

// Remove the specified compression extension. Error when the extension is not correct.
String *compressExtStrip(const String *file, CompressType type);

#endif
