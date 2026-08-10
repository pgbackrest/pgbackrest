/***********************************************************************************************************************************
Cipher Spec

Everything needed to encrypt or decrypt, kept together so that adding to it does not mean changing every function and protocol
message that carries it.

The passphrase (pass) contains the bytes the key is derived from. The digest is also here because the two are chosen together and
deriving from the wrong digest produces a wrong key rather than an error.

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
// Create from a pass and optionally a digest
typedef struct CipherSpecNewParam
{
    VAR_PARAM_HEADER;
    HashType digest;                                                // Digest to derive the key with instead of the default
} CipherSpecNewParam;

#define cipherSpecNewP(type, pass, ...)                                                                                            \
    cipherSpecNew(type, pass, (CipherSpecNewParam){VAR_PARAM_INIT, __VA_ARGS__})

FN_EXTERN CipherSpec *cipherSpecNew(CipherType type, const Buffer *pass, CipherSpecNewParam param);

// Create for no encryption
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
