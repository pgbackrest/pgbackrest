/***********************************************************************************************************************************
Cipher Spec

Everything needed to encrypt or decrypt, kept together so that adding to it does not mean changing every function and protocol
message that contains it.

The passphrase (pass) contains the bytes the key is derived from. There is no pass when the type is none, and the pass is never
logged.
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
// Create from a pass
FN_EXTERN CipherSpec *cipherSpecNew(CipherType type, const Buffer *pass);

// Create for no encryption
FN_INLINE_ALWAYS CipherSpec *
cipherSpecNewNone(void)
{
    return cipherSpecNew(cipherTypeNone, NULL);
}

// Create from a pack written by cipherSpecPack()
FN_EXTERN CipherSpec *cipherSpecNewPack(PackRead *packRead);

/***********************************************************************************************************************************
Getters/Setters
***********************************************************************************************************************************/
typedef struct CipherSpecPub
{
    CipherType type;                                                // Cipher type, none when not encrypted
    const Buffer *pass;                                             // Passphrase text or key bytes
} CipherSpecPub;

// Cipher type
FN_INLINE_ALWAYS CipherType
cipherSpecType(const CipherSpec *const this)
{
    return THIS_PUB(CipherSpec)->type;
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
    return cipherSpecNew(cipherSpecType(this), cipherSpecPass(this));
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
