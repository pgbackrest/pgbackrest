/***********************************************************************************************************************************
Cryptographic Hashes XS Header
***********************************************************************************************************************************/
#include "common/crypto/hash.h"
#include "common/memContext.h"

typedef struct CryptoHashXs
{
    MemContext *memContext;
    CryptoHash *pxPayload;
} CryptoHashXs, *pgBackRest__LibC__Crypto__Hash;
