/***********************************************************************************************************************************
Compression Helper

Abstract compression types so that the calling code does not need to worry about the individual characteristics of the compression
type. All compress calls should use this module rather than the individual filters, even if a specific compression type is required.
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
    compressTypeBz2,                                                // bzip2
    compressTypeGz,                                                 // gzip
    compressTypeLz4,                                                // lz4
    compressTypeZst,                                                // zstandard

    // These types have not been implemented but are included here so older versions can identify compression types added by future
    // versions. In that sense this list is speculative, but these seem to be all the types that are likely to be added in the
    // foreseeable future.
    compressTypeXz,                                                 // xz/lzma
} CompressType;

#include <common/io/filter/group.h>
#include <common/type/stringId.h>

/***********************************************************************************************************************************
Compression types as a regexp. In the future this regexp will be generated automatically at build time but we want to wait until the
build code is migrated to C to do that. For now just define it statically since we don't expect it to change very often. If a
supported type is not in this list then it should cause an integration test to fail.
***********************************************************************************************************************************/
#define COMPRESS_TYPE_REGEXP                                        "(\\.gz|\\.lz4|\\.zst|\\.xz|\\.bz2)"

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Get enum from a compression type string
FN_EXTERN CompressType compressTypeEnum(StringId type);

// Check that a valid compress type is compiled into this binary.  Errors when the compress type is not present.
FN_EXTERN void compressTypePresent(CompressType type);

// Get string representation of a compression type. This is the extension without the period.
FN_EXTERN const String *compressTypeStr(CompressType type);

// Get compression type from a (file) name by checking the extension. If the extension is not a supported compression type then
// compressType none is returned, even if the file is compressed with some unknown type.
FN_EXTERN CompressType compressTypeFromName(const String *name);

// Compression filter for the specified type.  Error when compress type is none or invalid.
FN_EXTERN IoFilter *compressFilter(CompressType type, int level);

// Compression/decompression filter based on string type and a parameter list.  This is useful when a filter must be created on a
// remote system since the filter type and parameters can be passed through a protocol.
FN_EXTERN IoFilter *compressFilterPack(StringId filterType, const Pack *filterParam);

// Decompression filter for the specified type.  Error when compress type is none or invalid.
FN_EXTERN IoFilter *decompressFilter(CompressType type);

// Get extension for the current compression type
FN_EXTERN const String *compressExtStr(CompressType type);

// Add extension for current compression type to the file
FN_EXTERN void compressExtCat(String *file, CompressType type);

// Remove the specified compression extension. Error when the extension is not correct.
FN_EXTERN String *compressExtStrip(const String *file, CompressType type);

#endif
