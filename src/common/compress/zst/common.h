/***********************************************************************************************************************************
ZST Common

Developed using the documentation in https://github.com/facebook/zstd/blob/v1.0.0/lib/zstd.h.
***********************************************************************************************************************************/
#ifndef COMMON_COMPRESS_ZST_COMMON_H
#define COMMON_COMPRESS_ZST_COMMON_H

#include <stddef.h>

/***********************************************************************************************************************************
ZST extension
***********************************************************************************************************************************/
#define ZST_EXT                                                     "zst"

#ifdef HAVE_LIBZST

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
FV_EXTERN size_t zstError(size_t error);

#endif // HAVE_LIBZST

#endif
