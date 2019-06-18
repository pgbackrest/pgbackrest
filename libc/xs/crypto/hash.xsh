/***********************************************************************************************************************************
Cryptographic Hashes XS Header
***********************************************************************************************************************************/
#include "common/crypto/hash.h"
#include "common/io/filter/filter.intern.h"
#include "common/memContext.h"

typedef struct CryptoHashXs
{
    MemContext *memContext;
    IoFilter *pxPayload;
} CryptoHashXs, *pgBackRest__LibC__Crypto__Hash;
