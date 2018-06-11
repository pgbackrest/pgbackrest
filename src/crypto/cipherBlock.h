/***********************************************************************************************************************************
Block Cipher Header
***********************************************************************************************************************************/
#ifndef CRYPTO_CIPHERBLOCK_H
#define CRYPTO_CIPHERBLOCK_H

/***********************************************************************************************************************************
CipherBlock object
***********************************************************************************************************************************/
typedef struct CipherBlock CipherBlock;

#include "crypto/cipher.h"

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
CipherBlock *cipherBlockNew(
    CipherMode mode, const char *cipherName, const unsigned char *pass, size_t passSize, const char *digestName);
size_t cipherBlockProcessSize(CipherBlock *this, size_t sourceSize);
size_t cipherBlockProcess(CipherBlock *this, const unsigned char *source, size_t sourceSize, unsigned char *destination);
size_t cipherBlockFlush(CipherBlock *this, unsigned char *destination);
void cipherBlockFree(CipherBlock *this);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_DEBUG_CIPHER_BLOCK_TYPE                                                                                           \
    CipherBlock *
#define FUNCTION_DEBUG_CIPHER_BLOCK_FORMAT(value, buffer, bufferSize)                                                              \
    objToLog(value, "CipherBlock", buffer, bufferSize)

#endif
