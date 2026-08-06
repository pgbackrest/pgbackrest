/***********************************************************************************************************************************
Cipher Spec

Everything needed to encrypt or decrypt, kept together so that adding to it does not mean changing every function and protocol
message that carries it.

The pass is the bytes the key is derived from rather than the text it was stored as. Whatever reads a pass from the repository
decides how to interpret it and builds a cipher spec from the result, so nothing downstream needs to know how it was stored. The
digest travels with the pass because the two are chosen together and deriving with the wrong digest produces a wrong key rather
than an error.

The pass is a buffer rather than a string so an absent pass is simply NULL, which saves callers from guarding a conversion that
cannot represent one. It is copied into the object, so the caller is free to release whatever it was read from.

The digest defaults to SHA-256, so a caller that has no reason to choose gets the digest new work should use. Deriving with SHA-1
is what every repository did before repository format 6 and is now specified explicitly, which also marks the places that are
waiting on a way to tell an old pass from a new one.

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
// Create from a pass, which is the key bytes or the passphrase text rather than what either was stored as
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

// Duplicate
FN_EXTERN CipherSpec *cipherSpecDup(const CipherSpec *this);

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
