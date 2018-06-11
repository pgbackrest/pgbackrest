/***********************************************************************************************************************************
Cryptographic Hashes
***********************************************************************************************************************************/
#ifndef CRYPTO_HASH_H
#define CRYPTO_HASH_H

/***********************************************************************************************************************************
Hash object
***********************************************************************************************************************************/
typedef struct CryptoHash CryptoHash;

#include "common/type/string.h"

/***********************************************************************************************************************************
Hash types
***********************************************************************************************************************************/
#define HASH_TYPE_MD5                                               "md5"
#define HASH_TYPE_SHA1                                              "sha1"
#define HASH_TYPE_SHA256                                            "sha256"

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
CryptoHash *cryptoHashNew(const String *type);
void cryptoHashProcess(CryptoHash *this, Buffer *message);
void cryptoHashProcessC(CryptoHash *this, const unsigned char *message, size_t messageSize);
void cryptoHashProcessStr(CryptoHash *this, String *message);
const Buffer *cryptoHash(CryptoHash *this);
String *cryptoHashHex(CryptoHash *this);
void cryptoHashFree(CryptoHash *this);

String *cryptoHashOne(const String *type, Buffer *message);
String *cryptoHashOneC(const String *type, const unsigned char *message, size_t messageSize);
String *cryptoHashOneStr(const String *type, String *message);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_DEBUG_CRYPTO_HASH_TYPE                                                                                           \
    CryptoHash *
#define FUNCTION_DEBUG_CRYPTO_HASH_FORMAT(value, buffer, bufferSize)                                                              \
    objToLog(value, "CryptoHash", buffer, bufferSize)

#endif
