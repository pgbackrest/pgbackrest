/***********************************************************************************************************************************
Cryptographic Hash

Generate a hash (sha1, md5, etc.) from a string, Buffer, or using an IoFilter.
***********************************************************************************************************************************/
#ifndef COMMON_CRYPTO_HASH_H
#define COMMON_CRYPTO_HASH_H

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
Hashes for zero-length files (i.e., starting hash)
***********************************************************************************************************************************/
#define HASH_TYPE_MD5_ZERO                                          "d41d8cd98f00b204e9800998ecf8427e"
#define HASH_TYPE_SHA1_ZERO                                         "da39a3ee5e6b4b0d3255bfef95601890afd80709"
    STRING_DECLARE(HASH_TYPE_SHA1_ZERO_STR);
#define HASH_TYPE_SHA256_ZERO                                                                                                      \
    "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"
    STRING_DECLARE(HASH_TYPE_SHA256_ZERO_STR);

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
Constructors
***********************************************************************************************************************************/
IoFilter *cryptoHashNew(const String *type);
IoFilter *cryptoHashNewVar(const VariantList *paramList);

/***********************************************************************************************************************************
Helper functions
***********************************************************************************************************************************/
// Get hash for one buffer
Buffer *cryptoHashOne(const String *type, const Buffer *message);

// Get hmac for one message/key
Buffer *cryptoHmacOne(const String *type, const Buffer *key, const Buffer *message);

#endif
