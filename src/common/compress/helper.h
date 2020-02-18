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
// Init compression settings from options
void compressInit(bool compress, const String *type, int level);

// Add compression filter to a filter group based on current settings.  If compression type is none then no filter will be added.
bool compressFilterAdd(IoFilterGroup *filterGroup);

// Get extension for the current compression type
const char *compressExtZ(void);

// Add extension for current compression type to the file
void compressExtCat(String *file);

#endif
