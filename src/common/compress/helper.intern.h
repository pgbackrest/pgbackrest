/***********************************************************************************************************************************
Compression Helper Internal
***********************************************************************************************************************************/
#ifndef COMMON_COMPRESS_HELPER_INTERN_H
#define COMMON_COMPRESS_HELPER_INTERN_H

#include "common/compress/helper.h"

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Default compression level for a compression type, used while loading the configuration
FN_EXTERN int compressLevelDefault(CompressType type);

// Minimum compression level for a compression type, used while loading the configuration
FN_EXTERN int compressLevelMin(CompressType type);

// Maximum compression level for a compression type, used while loading the configuration
FN_EXTERN int compressLevelMax(CompressType type);

#endif
