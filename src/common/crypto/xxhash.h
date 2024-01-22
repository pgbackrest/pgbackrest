/***********************************************************************************************************************************
xxHash Interface

Generate a 128-bit hash and return the specified number of bytes from the canonical representation. The 128-bit variant is only
slightly slower than the 64-bit variant so this makes getting up to 16 bytes of hash value fairly simple.

We consider this safe because xxHash claims to have excellent dispersion, zstd uses the lower 32-bits of a 64-bit hash for content
checksums, xxHash has been tested with SMHasher, and the "128-bit variant has better mixing and strength than the 64-bit variant,
even without counting the significantly larger output size".
***********************************************************************************************************************************/
#ifndef COMMON_CRYPTO_XXHASH_H
#define COMMON_CRYPTO_XXHASH_H

#include "common/io/filter/filter.h"
#include "common/type/buffer.h"

/***********************************************************************************************************************************
Filter type constant
***********************************************************************************************************************************/
#define XX_HASH_FILTER_TYPE                                         STRID5("xxhash", 0x1130a3180)

/***********************************************************************************************************************************
Max size of hash
***********************************************************************************************************************************/
#define XX_HASH_SIZE_MAX                                            16

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
FN_EXTERN IoFilter *xxHashNew(size_t size);

/***********************************************************************************************************************************
Helper functions
***********************************************************************************************************************************/
// Get hash for one buffer
FN_EXTERN Buffer *xxHashOne(size_t size, const Buffer *message);

#endif
