/***********************************************************************************************************************************
Cryptographic Hash

Generate a hash (sha1, md5, etc.) from a string, Buffer, or using an IoFilter.
***********************************************************************************************************************************/
#ifndef COMMON_CRYPTO_HASH_H
#define COMMON_CRYPTO_HASH_H

#include "common/crypto/common.h"
#include "common/io/filter/filter.h"
#include "common/type/string.h"

/***********************************************************************************************************************************
Filter type constant
***********************************************************************************************************************************/
#define CRYPTO_HASH_FILTER_TYPE                                     STRID5("hash", 0x44c280)

/***********************************************************************************************************************************
Hashes for zero-length files (i.e., starting hash)
***********************************************************************************************************************************/
#define HASH_TYPE_MD5_ZERO                                          "d41d8cd98f00b204e9800998ecf8427e"
#define HASH_TYPE_SHA1_ZERO                                         "da39a3ee5e6b4b0d3255bfef95601890afd80709"
BUFFER_DECLARE(HASH_TYPE_SHA1_ZERO_BUF);
#define HASH_TYPE_SHA256_ZERO                                                                                                      \
    "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"
BUFFER_DECLARE(HASH_TYPE_SHA256_ZERO_BUF);

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
FN_EXTERN IoFilter *cryptoHashNew(HashType type);
FN_EXTERN IoFilter *cryptoHashNewPack(const Pack *paramList);

/***********************************************************************************************************************************
Helper functions
***********************************************************************************************************************************/
// Get hash for one buffer
FN_EXTERN Buffer *cryptoHashOne(HashType type, const Buffer *message);

// Get hmac for one message/key
FN_EXTERN Buffer *cryptoHmacOne(HashType type, const Buffer *key, const Buffer *message);

#endif
