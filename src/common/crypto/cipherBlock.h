/***********************************************************************************************************************************
Block Cipher Header
***********************************************************************************************************************************/
#ifndef COMMON_CRYPTO_CIPHERBLOCK_H
#define COMMON_CRYPTO_CIPHERBLOCK_H

#include "common/crypto/spec.h"
#include "common/io/filter/group.h"

/***********************************************************************************************************************************
Filter type constant
***********************************************************************************************************************************/
#define CIPHER_BLOCK_FILTER_TYPE                                   STRID5("cipher-blk", 0x16c16e45441230)

/***********************************************************************************************************************************
Magic constant for salted encrypt, written before the salt unless the cipher is raw. Only salted encrypt is done here, but this
constant is required for compatibility with the openssl command-line tool. It is exposed so that whatever puts something of its own
in front of the salt can tell the two apart and report a file that has neither the way this filter would.
***********************************************************************************************************************************/
#define CIPHER_BLOCK_MAGIC                                          "Salted__"
#define CIPHER_BLOCK_MAGIC_SIZE                                     (sizeof(CIPHER_BLOCK_MAGIC) - 1)

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
typedef struct CipherBlockNewParam
{
    VAR_PARAM_HEADER;
    bool raw;                                                       // Omit header magic to save space
} CipherBlockNewParam;

#define cipherBlockNewP(mode, cipherSpec, ...)                                                                                     \
    cipherBlockNew(mode, cipherSpec, (CipherBlockNewParam){VAR_PARAM_INIT, __VA_ARGS__})

FN_EXTERN IoFilter *cipherBlockNew(CipherMode mode, const CipherSpec *cipherSpec, CipherBlockNewParam param);
FN_EXTERN IoFilter *cipherBlockNewPack(const Pack *paramList);

/***********************************************************************************************************************************
Helper functions
***********************************************************************************************************************************/
// Add a block cipher to an io object. Nothing is added when the repository is not encrypted.
FN_EXTERN IoFilterGroup *cipherBlockFilterGroupAdd(IoFilterGroup *filterGroup, CipherMode mode, const CipherSpec *cipherSpec);

#endif
