/***********************************************************************************************************************************
Cryptographic Hashes XS Header
***********************************************************************************************************************************/
#include "common/memContext.h"
#include "crypto/hash.h"

typedef struct CryptoHashXs
{
    MemContext *memContext;
    CryptoHash *pxPayload;
} CryptoHashXs, *pgBackRest__LibC__Crypto__Hash;
