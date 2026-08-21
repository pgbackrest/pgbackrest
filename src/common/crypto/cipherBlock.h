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
typedef struct CipherBlockNewParam
{
    VAR_PARAM_HEADER;
    bool raw;                                                       // Omit header magic to save space
    bool header;                                                    // Read the format header, decrypt only
    unsigned int format;                                            // Repository format, which on encrypt defines the header
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
// written at one, since the format defines the digest and whether a header is written.
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
