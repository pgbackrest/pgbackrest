/***********************************************************************************************************************************
Block Cipher XS Header
***********************************************************************************************************************************/
#include "common/assert.h"
#include "common/crypto/cipherBlock.h"
#include "common/io/io.h"
#include "common/memContext.h"

// Encrypt/decrypt modes
#define CIPHER_MODE_ENCRYPT                                         ((int)cipherModeEncrypt)
#define CIPHER_MODE_DECRYPT                                         ((int)cipherModeDecrypt)

typedef struct CipherBlockXs
{
    MemContext *memContext;
    IoFilter *pxPayload;
} CipherBlockXs, *pgBackRest__LibC__Cipher__Block;
