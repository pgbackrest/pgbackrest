/***********************************************************************************************************************************
Crypto Common Internal

Openssl types are used here, so this is included only by the crypto modules that work with them rather than by everything that
needs a hash type.
***********************************************************************************************************************************/
#ifndef COMMON_CRYPTO_COMMON_INTERN_H
#define COMMON_CRYPTO_COMMON_INTERN_H

#include <openssl/evp.h>

#include "common/crypto/common.h"

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Digest openssl knows by the name of the hash type. Error when there is no such digest, which is a coding error since a hash type
// is not read from user input.
FN_EXTERN const EVP_MD *cryptoDigest(HashType type);

#endif
