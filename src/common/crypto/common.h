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
