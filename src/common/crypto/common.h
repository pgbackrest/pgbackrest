/***********************************************************************************************************************************
Crypto Common
***********************************************************************************************************************************/
#ifndef COMMON_CRYPTO_COMMON_H
#define COMMON_CRYPTO_COMMON_H

#include <common/type/stringId.h>

/***********************************************************************************************************************************
Cipher modes
***********************************************************************************************************************************/
typedef enum
{
    cipherModeEncrypt,
    cipherModeDecrypt,
} CipherMode;

/***********************************************************************************************************************************
Cipher types
***********************************************************************************************************************************/
typedef enum
{
    cipherTypeNone = STRID5("none", 0x2b9ee0),
    cipherTypeAes256Cbc = STRID5("aes-256-cbc", 0xc43dfbbcdcca10),
} CipherType;

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Initialize crypto
void cryptoInit(void);

// Has crypto been initialized?
bool cryptoIsInit(void);

// Throw crypto errors
void cryptoError(bool error, const char *description);
void cryptoErrorCode(unsigned long code, const char *description) __attribute__((__noreturn__));

// Generate random bytes
void cryptoRandomBytes(unsigned char *buffer, size_t size);

#endif
