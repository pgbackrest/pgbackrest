/***********************************************************************************************************************************
Block Cipher Header
***********************************************************************************************************************************/
#ifndef CIPHER_BLOCK_H
#define CIPHER_BLOCK_H

#include "cipher/cipher.h"

// Track cipher state
typedef struct CipherBlock CipherBlock;

// Functions
CipherBlock *cipherBlockNew(
    CipherMode mode, const char *cipherName, const unsigned char *pass, int passSize, const char *digestName);
int cipherBlockProcessSize(CipherBlock *this, int sourceSize);
int cipherBlockProcess(CipherBlock *this, const unsigned char *source, int sourceSize, unsigned char *destination);
int cipherBlockFlush(CipherBlock *this, unsigned char *destination);
void cipherBlockFree(CipherBlock *this);

#endif
