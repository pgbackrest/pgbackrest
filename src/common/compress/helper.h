/***********************************************************************************************************************************
Compression Helper
***********************************************************************************************************************************/
#ifndef COMMON_COMPRESS_HELPER_H
#define COMMON_COMPRESS_HELPER_H

#include <stdbool.h>

#include <common/type/string.h>
#include <common/io/filter/group.h>

/***********************************************************************************************************************************
Available compression types
***********************************************************************************************************************************/
typedef enum
{
    compressTypeNone,
    compressTypeGzip,
    compressTypeLz4,
} CompressType;

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Get enum from a compression type string
CompressType compressTypeEnum(const String *type);

// Get string representation of a compression type.  This should be the extension.
const char *compressTypeZ(CompressType type);

// Get compression type from a (file) name by checking the extension.  If the extension is not a supported compression type then
// compressType none is returned, even if the file is compressed with some unknown type.
CompressType compressTypeFromName(const String *name);

// Add compression filter to a filter group.  If compression type is none then no filter will be added.
bool compressFilterAdd(IoFilterGroup *filterGroup, CompressType type, int level);

// Add decompression filter to a filter group.  If compression type is none then no filter will be added.
bool decompressFilterAdd(IoFilterGroup *filterGroup, CompressType type);

// Get extension for the current compression type
const char *compressExtZ(CompressType type);

// Add extension for current compression type to the file
void compressExtCat(String *file, CompressType type);

// Remove the specified compression extension.  Error when the extension is not correct.
String *compressExtStrip(const String *file, CompressType type);

#endif
