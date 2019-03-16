/***********************************************************************************************************************************
Cryptographic Hash

Generate a hash (sha1, md5, etc.) from a string, Buffer, or using an IoFilter.
***********************************************************************************************************************************/
#ifndef COMMON_CRYPTO_HASH_H
#define COMMON_CRYPTO_HASH_H

/***********************************************************************************************************************************
Hash object
***********************************************************************************************************************************/
typedef struct CryptoHash CryptoHash;

#include "common/io/filter/filter.h"
#include "common/type/string.h"

/***********************************************************************************************************************************
Filter type constant
***********************************************************************************************************************************/
#define CRYPTO_HASH_FILTER_TYPE                                     "hash"
    STRING_DECLARE(CRYPTO_HASH_FILTER_TYPE_STR);

/***********************************************************************************************************************************
Hash types
***********************************************************************************************************************************/
#define HASH_TYPE_MD5                                               "md5"
    STRING_DECLARE(HASH_TYPE_MD5_STR);
#define HASH_TYPE_SHA1                                              "sha1"
    STRING_DECLARE(HASH_TYPE_SHA1_STR);
#define HASH_TYPE_SHA256                                            "sha256"
    STRING_DECLARE(HASH_TYPE_SHA256_STR);

/***********************************************************************************************************************************
Hash type sizes
***********************************************************************************************************************************/
#define HASH_TYPE_M5_SIZE                                           16
#define HASH_TYPE_MD5_SIZE_HEX                                      (HASH_TYPE_M5_SIZE * 2)

#define HASH_TYPE_SHA1_SIZE                                         20
#define HASH_TYPE_SHA1_SIZE_HEX                                     (HASH_TYPE_SHA1_SIZE * 2)

#define HASH_TYPE_SHA256_SIZE                                       32
#define HASH_TYPE_SHA256_SIZE_HEX                                   (HASH_TYPE_SHA256_SIZE * 2)

/***********************************************************************************************************************************
Constructor
***********************************************************************************************************************************/
CryptoHash *cryptoHashNew(const String *type);

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
void cryptoHashProcess(CryptoHash *this, const Buffer *message);
void cryptoHashProcessC(CryptoHash *this, const unsigned char *message, size_t messageSize);
void cryptoHashProcessStr(CryptoHash *this, const String *message);

/***********************************************************************************************************************************
Getters
***********************************************************************************************************************************/
const Buffer *cryptoHash(CryptoHash *this);
IoFilter *cryptoHashFilter(CryptoHash *this);
const Variant *cryptoHashResult(CryptoHash *this);

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
void cryptoHashFree(CryptoHash *this);

/***********************************************************************************************************************************
Helper functions
***********************************************************************************************************************************/
Buffer *cryptoHashOne(const String *type, const Buffer *message);
Buffer *cryptoHashOneC(const String *type, const unsigned char *message, size_t messageSize);
Buffer *cryptoHashOneStr(const String *type, const String *message);

Buffer *cryptoHmacOne(const String *type, const Buffer *key, const Buffer *message);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_CRYPTO_HASH_TYPE                                                                                              \
    CryptoHash *
#define FUNCTION_LOG_CRYPTO_HASH_FORMAT(value, buffer, bufferSize)                                                                 \
    objToLog(value, "CryptoHash", buffer, bufferSize)

#endif
