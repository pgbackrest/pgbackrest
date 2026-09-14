/***********************************************************************************************************************************
Block Cipher Header
***********************************************************************************************************************************/
#ifndef COMMON_CRYPTO_CIPHERBLOCK_H
#define COMMON_CRYPTO_CIPHERBLOCK_H

#include "common/crypto/spec.h"
#include "common/io/filter/group.h"

/***********************************************************************************************************************************
Magic constant for salted encrypt, written before the salt unless the header is none. Anything written in place of the magic must be
the same size so that what follows still begins with the salt.
***********************************************************************************************************************************/
#define CIPHER_BLOCK_MAGIC                                          "Salted__"
#define CIPHER_BLOCK_MAGIC_SIZE                                     (sizeof(CIPHER_BLOCK_MAGIC) - 1)

/***********************************************************************************************************************************
Filter type constant
***********************************************************************************************************************************/
#define CIPHER_BLOCK_FILTER_TYPE                                   STRID5("cipher-blk", 0x16c16e45441230)

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
// What precedes the content of a file. The magic is what openssl writes and is the default. None saves the eight bytes the magic
// takes for a file nothing but this version reads, e.g. a file bundled into a backup, and for a file that something else has
// written in place of the magic, e.g. the format header.
typedef enum
{
    cipherBlockHeaderMagic = 0,                                     // Salted magic openssl writes
    cipherBlockHeaderNone,                                          // Nothing, to save space
} CipherBlockHeader;

typedef struct CipherBlockNewParam
{
    VAR_PARAM_HEADER;
    CipherBlockHeader header;                                       // What precedes the content, the magic by default
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
