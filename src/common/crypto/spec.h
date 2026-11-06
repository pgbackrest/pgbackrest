/***********************************************************************************************************************************
Cipher Spec

Everything needed to encrypt or decrypt, kept together so that adding to it does not mean changing every function and protocol
message that contains it.

The pass holds the bytes used to derive the key. The digest travels with the pass because the two are chosen together and deriving
with the wrong digest gives a wrong key instead of an error.

The pass is a buffer rather than a string because it may be binary or it may be text and nothing here needs to know which. It is
copied into the object, so the caller can release whatever it read the pass from.

An unset digest means the repository format of the file being read or written specifies it.

There is no digest or pass when the type is none, and the pass is never logged.
***********************************************************************************************************************************/
#ifndef COMMON_CRYPTO_SPEC_H
#define COMMON_CRYPTO_SPEC_H

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct CipherSpec CipherSpec;

#include "common/crypto/common.h"
#include "common/type/buffer.h"
#include "common/type/object.h"
#include "common/type/pack.h"

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
// Create from a pass, which is the key bytes or the passphrase text
typedef struct CipherSpecNewParam
{
    VAR_PARAM_HEADER;
    HashType digest;                                                // Digest to derive the key with, unset when specified by format
} CipherSpecNewParam;

#define cipherSpecNewP(type, pass, ...)                                                                                            \
    cipherSpecNew(type, pass, (CipherSpecNewParam){VAR_PARAM_INIT, __VA_ARGS__})

FN_EXTERN CipherSpec *cipherSpecNew(CipherType type, const Buffer *pass, CipherSpecNewParam param);

// Create for data that is not encrypted
FN_INLINE_ALWAYS CipherSpec *
cipherSpecNewNone(void)
{
    return cipherSpecNewP(cipherTypeNone, NULL);
}

// Create from a pack written by cipherSpecPack()
FN_EXTERN CipherSpec *cipherSpecNewPack(PackRead *packRead);

/***********************************************************************************************************************************
Getters/Setters
***********************************************************************************************************************************/
typedef struct CipherSpecPub
{
    CipherType type;                                                // Cipher type, none when not encrypted
    HashType digest;                                                // Digest the pass derives with, unset when specified by format
    const Buffer *pass;                                             // Passphrase text or key bytes
} CipherSpecPub;

// Cipher type
FN_INLINE_ALWAYS CipherType
cipherSpecType(const CipherSpec *const this)
{
    return THIS_PUB(CipherSpec)->type;
}

// Digest the pass derives the key with, unset when specified by the format
FN_INLINE_ALWAYS HashType
cipherSpecDigest(const CipherSpec *const this)
{
    return THIS_PUB(CipherSpec)->digest;
}

// Passphrase text or key bytes
FN_INLINE_ALWAYS const Buffer *
cipherSpecPass(const CipherSpec *const this)
{
    return THIS_PUB(CipherSpec)->pass;
}

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Duplicate
typedef struct CipherSpecDupParam
{
    VAR_PARAM_HEADER;
    HashType digestDefault;                                         // Digest to use when the spec has none
} CipherSpecDupParam;

#define cipherSpecDupP(this, ...)                                                                                                  \
    cipherSpecDup(this, (CipherSpecDupParam){VAR_PARAM_INIT, __VA_ARGS__})

FN_EXTERN CipherSpec *cipherSpecDup(const CipherSpec *this, CipherSpecDupParam param);

// Write to a pack so it can be passed over a protocol
FN_EXTERN void cipherSpecPack(PackWrite *packWrite, const CipherSpec *this);

// Move to a new parent mem context
FN_INLINE_ALWAYS CipherSpec *
cipherSpecMove(CipherSpec *const this, MemContext *const parentNew)
{
    return objMove(this, parentNew);
}

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
FN_INLINE_ALWAYS void
cipherSpecFree(CipherSpec *const this)
{
    objFree(this);
}

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
FN_EXTERN void cipherSpecToLog(const CipherSpec *this, StringStatic *debugLog);

#define FUNCTION_LOG_CIPHER_SPEC_TYPE                                                                                              \
    CipherSpec *
#define FUNCTION_LOG_CIPHER_SPEC_FORMAT(value, buffer, bufferSize)                                                                 \
    FUNCTION_LOG_OBJECT_FORMAT(value, cipherSpecToLog, buffer, bufferSize)

#endif
