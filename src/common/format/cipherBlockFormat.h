/***********************************************************************************************************************************
Cipher Block Format

Encryption of a file written at a repository format, which is the format header in front of the content and the digest the format
derives the key with. Everything the format defines is here so that the block cipher itself has no knowledge of the repository.

A writer knows the format, so it writes the header itself and encrypts the content raw behind it. A reader cannot know the format
until it has read the header, and a filter cannot be added to a read once it is open, so the read side is a filter that parses
the header, works out from it what the content is encrypted with, and passes everything after it through a block cipher of its own.
***********************************************************************************************************************************/
#ifndef COMMON_FORMAT_CIPHERBLOCKFORMAT_H
#define COMMON_FORMAT_CIPHERBLOCKFORMAT_H

#include "common/crypto/spec.h"
#include "common/io/filter/group.h"

/***********************************************************************************************************************************
Filter type constant
***********************************************************************************************************************************/
#define CIPHER_BLOCK_FORMAT_FILTER_TYPE                             STRID5("cipher-fmt", 0x28d36e45441230)

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
typedef struct CipherBlockFormatNewParam
{
    VAR_PARAM_HEADER;
    unsigned int format;                                            // Format the file is expected at, any when zero
} CipherBlockFormatNewParam;

#define cipherBlockFormatNewP(cipherSpec, ...)                                                                                     \
    cipherBlockFormatNew(cipherSpec, (CipherBlockFormatNewParam){VAR_PARAM_INIT, __VA_ARGS__})

FN_EXTERN IoFilter *cipherBlockFormatNew(const CipherSpec *cipherSpec, CipherBlockFormatNewParam param);
FN_EXTERN IoFilter *cipherBlockFormatNewPack(const Pack *paramList);

/***********************************************************************************************************************************
Getters/Setters
***********************************************************************************************************************************/
// The format the header gave, which is what the header was read for
FN_EXTERN unsigned int cipherBlockFormatResult(PackRead *packRead);

/***********************************************************************************************************************************
Helper functions
***********************************************************************************************************************************/
// Write the header a format calls for into the buffer the content will be written into and add encryption to the filter group.
// Nothing is written and nothing is added when the repository is not encrypted.
FN_EXTERN void cipherBlockFormatFilterGroupWriteAdd(
    Buffer *buffer, IoFilterGroup *filterGroup, const CipherSpec *cipherSpec, unsigned int format);

// Add decryption to the filter group for a file at a format that has yet to be read. Nothing is added when the repository is not
// encrypted.
FN_EXTERN IoFilterGroup *cipherBlockFormatFilterGroupReadAdd(IoFilterGroup *filterGroup, const CipherSpec *cipherSpec);

#endif
