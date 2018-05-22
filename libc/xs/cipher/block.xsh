/***********************************************************************************************************************************
Block Cipher XS Header
***********************************************************************************************************************************/
#include "common/memContext.h"
#include "cipher/block.h"

// Encrypt/decrypt modes
#define CIPHER_MODE_ENCRYPT                                         ((int)cipherModeEncrypt)
#define CIPHER_MODE_DECRYPT                                         ((int)cipherModeDecrypt)

typedef struct CipherBlockXs
{
    MemContext *memContext;
    CipherBlock *pxPayload;
} CipherBlockXs, *pgBackRest__LibC__Cipher__Block;
