/***********************************************************************************************************************************
Block Cipher Header
***********************************************************************************************************************************/
#ifndef COMMON_CRYPTO_CIPHERBLOCK_H
#define COMMON_CRYPTO_CIPHERBLOCK_H

#include "common/io/filter/group.h"
#include "common/crypto/common.h"

/***********************************************************************************************************************************
Filter type constant
***********************************************************************************************************************************/
#define CIPHER_BLOCK_FILTER_TYPE                                   "cipherBlock"
    STRING_DECLARE(CIPHER_BLOCK_FILTER_TYPE_STR);

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
IoFilter *cipherBlockNew(CipherMode mode, uint64_t cipherType, const Buffer *pass, const String *digestName);
IoFilter *cipherBlockNewVar(const VariantList *paramList);

/***********************************************************************************************************************************
Helper functions
***********************************************************************************************************************************/
// Add a block cipher to an io object
IoFilterGroup *cipherBlockFilterGroupAdd(IoFilterGroup *filterGroup, CipherType type, CipherMode mode, const String *pass);

#endif
