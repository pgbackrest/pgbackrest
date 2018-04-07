/***********************************************************************************************************************************
Block Cipher Header
***********************************************************************************************************************************/
#ifndef CIPHER_BLOCK_H
#define CIPHER_BLOCK_H

/***********************************************************************************************************************************
CipherBlock object
***********************************************************************************************************************************/
typedef struct CipherBlock CipherBlock;

#include "cipher/cipher.h"

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
CipherBlock *cipherBlockNew(
    CipherMode mode, const char *cipherName, const unsigned char *pass, size_t passSize, const char *digestName);
size_t cipherBlockProcessSize(CipherBlock *this, size_t sourceSize);
size_t cipherBlockProcess(CipherBlock *this, const unsigned char *source, size_t sourceSize, unsigned char *destination);
size_t cipherBlockFlush(CipherBlock *this, unsigned char *destination);
void cipherBlockFree(CipherBlock *this);

#endif
