/***********************************************************************************************************************************
Crypto Common
***********************************************************************************************************************************/
#ifndef COMMON_CRYPTO_COMMON_H
#define COMMON_CRYPTO_COMMON_H

#include "common/type/stringId.h"

/***********************************************************************************************************************************
Cipher modes
***********************************************************************************************************************************/
typedef enum
{
    cipherModeEncrypt = STRID5("enc", 0xdc50),
    cipherModeDecrypt = STRID5("dec", 0xca40),
} CipherMode;

/***********************************************************************************************************************************
Cipher types
***********************************************************************************************************************************/
typedef enum
{
    cipherTypeNone,
    cipherTypeAes256Cbc,
} CipherType;

#define CIPHER_TYPE_NONE                                            "none"
    STRING_DECLARE(CIPHER_TYPE_NONE_STR);
#define CIPHER_TYPE_AES_256_CBC                                     "aes-256-cbc"
    STRING_DECLARE(CIPHER_TYPE_AES_256_CBC_STR);

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

// Get cipher type or name
CipherType cipherType(const String *name);
const String *cipherTypeName(CipherType type);

// Generate random bytes
void cryptoRandomBytes(unsigned char *buffer, size_t size);

#endif
