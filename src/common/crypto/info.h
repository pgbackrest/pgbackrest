/***********************************************************************************************************************************
Cipher Info

Everything needed to encrypt or decrypt, kept together so that adding to it does not mean changing every function and protocol
message that carries it.

The pass is the bytes the key is derived from rather than the text it was stored as. Whatever reads a pass from the repository
decides how to interpret it and builds cipher info from the result, so nothing downstream needs to know how it was stored. The
digest travels with the pass because the two are chosen together and deriving with the wrong digest produces a wrong key rather
than an error.

The pass is a buffer rather than a string so an absent pass is simply NULL, which saves callers from guarding a conversion that
cannot represent one. It is copied into the object, so the caller is free to release whatever it was read from.

There is no digest or pass when the type is none, and the pass is never logged.
***********************************************************************************************************************************/
#ifndef COMMON_CRYPTO_INFO_H
#define COMMON_CRYPTO_INFO_H

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct CipherInfo CipherInfo;

#include "common/crypto/common.h"
#include "common/type/buffer.h"
#include "common/type/object.h"
#include "common/type/pack.h"

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
// Create from a pass, which is the key bytes or the passphrase text rather than what either was stored as
typedef struct CipherInfoNewParam
{
    VAR_PARAM_HEADER;
    HashType digest;                                                // Digest to derive the key with instead of the default
} CipherInfoNewParam;

#define cipherInfoNewP(type, pass, ...)                                                                                            \
    cipherInfoNew(type, pass, (CipherInfoNewParam){VAR_PARAM_INIT, __VA_ARGS__})

FN_EXTERN CipherInfo *cipherInfoNew(CipherType type, const Buffer *pass, CipherInfoNewParam param);

// Create for data that is not encrypted
FN_INLINE_ALWAYS CipherInfo *
cipherInfoNewNone(void)
{
    return cipherInfoNewP(cipherTypeNone, NULL);
}

// Create from a pack written by cipherInfoPack()
FN_EXTERN CipherInfo *cipherInfoNewPack(PackRead *packRead);

// Duplicate
FN_EXTERN CipherInfo *cipherInfoDup(const CipherInfo *this);

/***********************************************************************************************************************************
Getters/Setters
***********************************************************************************************************************************/
typedef struct CipherInfoPub
{
    CipherType type;                                                // Cipher type, none when not encrypted
    HashType digest;                                                // Digest the pass derives the key with
    const Buffer *pass;                                             // Passphrase text or key bytes
} CipherInfoPub;

// Cipher type
FN_INLINE_ALWAYS CipherType
cipherInfoType(const CipherInfo *const this)
{
    return THIS_PUB(CipherInfo)->type;
}

// Digest the pass derives the key with
FN_INLINE_ALWAYS HashType
cipherInfoDigest(const CipherInfo *const this)
{
    return THIS_PUB(CipherInfo)->digest;
}

// Passphrase text or key bytes
FN_INLINE_ALWAYS const Buffer *
cipherInfoPass(const CipherInfo *const this)
{
    return THIS_PUB(CipherInfo)->pass;
}

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Write to a pack so it can be passed over a protocol
FN_EXTERN void cipherInfoPack(PackWrite *packWrite, const CipherInfo *this);

// Move to a new parent mem context
FN_INLINE_ALWAYS CipherInfo *
cipherInfoMove(CipherInfo *const this, MemContext *const parentNew)
{
    return objMove(this, parentNew);
}

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
FN_INLINE_ALWAYS void
cipherInfoFree(CipherInfo *const this)
{
    objFree(this);
}

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
FN_EXTERN void cipherInfoToLog(const CipherInfo *this, StringStatic *debugLog);

#define FUNCTION_LOG_CIPHER_INFO_TYPE                                                                                              \
    CipherInfo *
#define FUNCTION_LOG_CIPHER_INFO_FORMAT(value, buffer, bufferSize)                                                                 \
    FUNCTION_LOG_OBJECT_FORMAT(value, cipherInfoToLog, buffer, bufferSize)

#endif
