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
Constructors
***********************************************************************************************************************************/
// What precedes the content of a file. The magic is what openssl writes and is the default. None saves the eight bytes the magic
// takes for a file nothing but this version reads, e.g. a file bundled into a backup. Format is the framing the repository format
// defines, which is the magic before format 6 and the format header from then on, and is passed by a reader that has yet to learn
// the format. A writer knows the format and passes it instead, so the framing follows from it.
typedef enum
{
    cipherBlockHeaderMagic = 0,                                     // Salted magic openssl writes
    cipherBlockHeaderNone,                                          // Nothing, to save space
    cipherBlockHeaderFormat,                                        // Whatever the format defines, decrypt only
} CipherBlockHeader;

typedef struct CipherBlockNewParam
{
    VAR_PARAM_HEADER;
    CipherBlockHeader header;                                       // What precedes the content, the magic by default
    unsigned int format;                                            // Repository format, which is what defines the header written
} CipherBlockNewParam;

#define cipherBlockNewP(mode, cipherSpec, ...)                                                                                     \
    cipherBlockNew(mode, cipherSpec, (CipherBlockNewParam){VAR_PARAM_INIT, __VA_ARGS__})

FN_EXTERN IoFilter *cipherBlockNew(CipherMode mode, const CipherSpec *cipherSpec, CipherBlockNewParam param);
FN_EXTERN IoFilter *cipherBlockNewPack(const Pack *paramList);

/***********************************************************************************************************************************
Getters/Setters
***********************************************************************************************************************************/
// The format the file was written with, which is what the header was read for. Only a filter that read a header has one to report.
FN_EXTERN unsigned int cipherBlockFormat(PackRead *cipherBlockResult);

/***********************************************************************************************************************************
Helper functions
***********************************************************************************************************************************/
// Add a block cipher to an io object. Nothing is added when the repository is not encrypted. The format is required when a file is
// written at one, since the format defines the digest and the header.
typedef struct CipherBlockFilterGroupAddParam
{
    VAR_PARAM_HEADER;
    unsigned int format;                                            // Repository format the file is written at
} CipherBlockFilterGroupAddParam;

#define cipherBlockFilterGroupAddP(filterGroup, mode, cipherSpec, ...)                                                             \
    cipherBlockFilterGroupAdd(filterGroup, mode, cipherSpec, (CipherBlockFilterGroupAddParam){VAR_PARAM_INIT, __VA_ARGS__})

FN_EXTERN IoFilterGroup *cipherBlockFilterGroupAdd(
    IoFilterGroup *filterGroup, CipherMode mode, const CipherSpec *cipherSpec, CipherBlockFilterGroupAddParam param);

#endif
