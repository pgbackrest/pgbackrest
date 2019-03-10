/***********************************************************************************************************************************
Block Cipher Header
***********************************************************************************************************************************/
#ifndef COMMON_CRYPTO_CIPHERBLOCK_H
#define COMMON_CRYPTO_CIPHERBLOCK_H

/***********************************************************************************************************************************
CipherBlock object
***********************************************************************************************************************************/
typedef struct CipherBlock CipherBlock;

#include "common/io/filter/filter.h"
#include "common/type/buffer.h"
#include "common/crypto/common.h"

/***********************************************************************************************************************************
Constructor
***********************************************************************************************************************************/
CipherBlock *cipherBlockNew(CipherMode mode, CipherType cipherType, const Buffer *pass, const String *digestName);

// Deprecated constructor with C-style call signatures now needed only for the Perl interface
CipherBlock *cipherBlockNewC(
    CipherMode mode, const char *cipherName, const unsigned char *pass, size_t passSize, const char *digestName);

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
void cipherBlockProcess(CipherBlock *this, const Buffer *source, Buffer *destination);

// Deprecated functions with C-style call signatures now needed only for the Perl interface
size_t cipherBlockProcessSizeC(CipherBlock *this, size_t sourceSize);
size_t cipherBlockProcessC(CipherBlock *this, const unsigned char *source, size_t sourceSize, unsigned char *destination);
size_t cipherBlockFlushC(CipherBlock *this, unsigned char *destination);

/***********************************************************************************************************************************
Getters
***********************************************************************************************************************************/
bool cipherBlockDone(const CipherBlock *this);
IoFilter *cipherBlockFilter(const CipherBlock *this);
bool cipherBlockInputSame(const CipherBlock *this);

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
void cipherBlockFree(CipherBlock *this);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
String *cipherBlockToLog(const CipherBlock *this);

#define FUNCTION_LOG_CIPHER_BLOCK_TYPE                                                                                             \
    CipherBlock *
#define FUNCTION_LOG_CIPHER_BLOCK_FORMAT(value, buffer, bufferSize)                                                                \
    FUNCTION_LOG_STRING_OBJECT_FORMAT(value, cipherBlockToLog, buffer, bufferSize)

#endif
