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
#define CIPHER_BLOCK_FILTER_TYPE                                   STRID5("cipher-blk", 0x16c16e45441230)

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
typedef struct CipherBlockNewParam
{
    VAR_PARAM_HEADER;
    const String *digest;                                           // Digest to use (defaults to SHA-1)
} CipherBlockNewParam;

#define cipherBlockNewP(mode, cipherType, pass, ...)                                                                               \
    cipherBlockNew(mode, cipherType, pass, (CipherBlockNewParam){VAR_PARAM_INIT, __VA_ARGS__})

IoFilter *cipherBlockNew(CipherMode mode, CipherType cipherType, const Buffer *pass, CipherBlockNewParam param);
IoFilter *cipherBlockNewPack(const Pack *paramList);

/***********************************************************************************************************************************
Helper functions
***********************************************************************************************************************************/
// Add a block cipher to an io object
IoFilterGroup *cipherBlockFilterGroupAdd(IoFilterGroup *filterGroup, CipherType type, CipherMode mode, const String *pass);

#endif
