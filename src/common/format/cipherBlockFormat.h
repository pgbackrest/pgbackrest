/***********************************************************************************************************************************
Cipher Block Format

Encryption of a file written at a repository format, which is the format header in front of the content and the digest the format
derives the key with. Everything the format defines is here so that the block cipher itself has no knowledge of the repository.

A writer knows the format, so it writes the header itself and encrypts the content raw behind it. A reader cannot know the format
until it has read the header, and a filter cannot be added to a read once it is open, so the read side is a filter that parses
the header, works out from it what the content is encrypted with, and passes everything after it through a block cipher of its own.

A file whose key cannot be determined from where the file is located stores the key in its header.
***********************************************************************************************************************************/
#ifndef COMMON_FORMAT_CIPHERBLOCKFORMAT_H
#define COMMON_FORMAT_CIPHERBLOCKFORMAT_H

#include "common/crypto/spec.h"
#include "common/format/cipherSpecMap.h"
#include "common/io/filter/group.h"

/***********************************************************************************************************************************
Filter type constants
***********************************************************************************************************************************/
#define CIPHER_BLOCK_FORMAT_FILTER_TYPE                             STRID5("cipher-fmt", 0x28d36e45441230)

// Filter that writes the format header
#define CIPHER_BLOCK_FORMAT_HEADER_FILTER_TYPE                      STRID5("cipher-hdr", 0x24446e45441230)

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
typedef struct CipherBlockFormatNewParam
{
    VAR_PARAM_HEADER;
    unsigned int format;                                            // Format the file is expected at, any when zero
} CipherBlockFormatNewParam;

// Decrypt with the stored key id or with CIPHER_SPEC_MAP_ID_DEFAULT when no key is stored
#define cipherBlockFormatNewP(cipherSpecMap, ...)                                                                                  \
    cipherBlockFormatNew(cipherSpecMap, (CipherBlockFormatNewParam){VAR_PARAM_INIT, __VA_ARGS__})

FN_EXTERN IoFilter *cipherBlockFormatNew(const CipherSpecMap *cipherSpecMap, CipherBlockFormatNewParam param);
FN_EXTERN IoFilter *cipherBlockFormatNewPack(const Pack *paramList);

// Create filter that writes the format header from its param list
FN_EXTERN IoFilter *cipherBlockFormatHeaderNewPack(const Pack *paramList);

/***********************************************************************************************************************************
Getters/Setters
***********************************************************************************************************************************/
// The format the header gave, which is what the header was read for
FN_EXTERN unsigned int cipherBlockFormatResult(PackRead *packRead);

/***********************************************************************************************************************************
Helper functions
***********************************************************************************************************************************/
// Write the header the format requires into the buffer the content will be written into and add encryption to the filter group. A
// key id is stored in the header when one is specified. No header is added when the repository is not encrypted.
typedef struct CipherBlockFormatFilterGroupWriteAddParam
{
    VAR_PARAM_HEADER;
    const String *keyId;                                            // Key id to store in the header, NULL to store no id
} CipherBlockFormatFilterGroupWriteAddParam;

#define cipherBlockFormatFilterGroupWriteAddP(filterGroup, cipherSpec, format, ...)                                                \
    cipherBlockFormatFilterGroupWriteAdd(                                                                                          \
        filterGroup, cipherSpec, format, (CipherBlockFormatFilterGroupWriteAddParam){VAR_PARAM_INIT, __VA_ARGS__})

FN_EXTERN void cipherBlockFormatFilterGroupWriteAdd(
    IoFilterGroup *filterGroup, const CipherSpec *cipherSpec, unsigned int format,
    CipherBlockFormatFilterGroupWriteAddParam param);

// Add decryption to the filter group for a file with format yet to be determined. The key is stored under
// CIPHER_SPEC_MAP_ID_DEFAULT, so the file must not contain a key id. Nothing is added when the repository is not encrypted.
FN_EXTERN IoFilterGroup *cipherBlockFormatFilterGroupReadAdd(IoFilterGroup *filterGroup, const CipherSpec *cipherSpec);

// Add decryption to the filter group for a file that may contain a key id. An empty map means the repository is not encrypted and
// nothing is added.
FN_EXTERN IoFilterGroup *cipherBlockFormatFilterGroupReadAddMap(IoFilterGroup *filterGroup, const CipherSpecMap *cipherSpecMap);

#endif
