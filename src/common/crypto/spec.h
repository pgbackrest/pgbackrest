/***********************************************************************************************************************************
Cipher Spec

Everything needed to encrypt or decrypt, kept together so that adding to it does not mean changing every function and protocol
message that contains it.

The pass holds the bytes used to derive the key. The digest travels with the pass because the two are chosen together and deriving
with the wrong digest gives a wrong key instead of an error.

The pass is a buffer rather than a string because it may be binary or it may be text and nothing here needs to know which. It is
copied into the object, so the caller can release whatever it read the pass from.

The digest defaults to SHA-256, so a caller with no preference gets the digest new work should use. Repositories before format 6
derived with SHA-1, so a caller that works with that format must pass SHA-1 rather than taking the default.

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
    HashType digest;                                                // Digest to derive the key with instead of SHA-256
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
    HashType digest;                                                // Digest the pass derives the key with
    const Buffer *pass;                                             // Passphrase text or key bytes
} CipherSpecPub;

// Cipher type
FN_INLINE_ALWAYS CipherType
cipherSpecType(const CipherSpec *const this)
{
    return THIS_PUB(CipherSpec)->type;
}

// Digest the pass derives the key with
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
FN_INLINE_ALWAYS CipherSpec *
cipherSpecDup(const CipherSpec *const this)
{
    return cipherSpecNewP(cipherSpecType(this), cipherSpecPass(this), .digest = cipherSpecDigest(this));
}

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
