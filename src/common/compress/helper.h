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
    compressTypeNone,
    compressTypeGzip,
    compressTypeLz4,

    // These types have not been implemented but are included here so older versions can identify compression types added by future
    // versions. In that sense this list is speculative, but these seem to be all the types that are likely to be added.
    compressTypeZst,
    compressTypeXz,
    compressTypeBz2,
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

// Add compression filter to a filter group. If compression type is none then no filter will be added.
bool compressFilterAdd(IoFilterGroup *filterGroup, CompressType type, int level);

// Add decompression filter to a filter group. If compression type is none then no filter will be added.
bool decompressFilterAdd(IoFilterGroup *filterGroup, CompressType type);

// Get extension for the current compression type
const char *compressExtZ(CompressType type);

// Add extension for current compression type to the file
void compressExtCat(String *file, CompressType type);

// Remove the specified compression extension. Error when the extension is not correct.
String *compressExtStrip(const String *file, CompressType type);

#endif
