/***********************************************************************************************************************************
Cipher Spec
***********************************************************************************************************************************/
#include <build.h>

#include "common/crypto/spec.h"
#include "common/debug.h"
#include "common/log.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct CipherSpec
{
    CipherSpecPub pub;                                              // Publicly accessible variables
};

/**********************************************************************************************************************************/
FN_EXTERN CipherSpec *
cipherSpecNew(const CipherType type, const Buffer *const pass)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING_ID, type);
        FUNCTION_TEST_PARAM(BUFFER, pass);
    FUNCTION_TEST_END();

    ASSERT((type == cipherTypeNone) == (pass == NULL));
    ASSERT(pass == NULL || !bufEmpty(pass));

    OBJ_NEW_BEGIN(CipherSpec, .childQty = MEM_CONTEXT_QTY_MAX)
    {
        *this = (CipherSpec){.pub = {.type = type}};

        if (this->pub.type != cipherTypeNone)
            this->pub.pass = bufDup(pass);
    }
    OBJ_NEW_END();

    FUNCTION_TEST_RETURN(CIPHER_SPEC, this);
}

/**********************************************************************************************************************************/
FN_EXTERN CipherSpec *
cipherSpecNewPack(PackRead *const packRead)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PACK_READ, packRead);
    FUNCTION_TEST_END();

    ASSERT(packRead != NULL);

    OBJ_NEW_BEGIN(CipherSpec, .childQty = MEM_CONTEXT_QTY_MAX)
    {
        *this = (CipherSpec){.pub = {.type = (CipherType)pckReadStrIdP(packRead)}};

        // Nothing else was written when there is no cipher. The pass is read here rather than copied in since this context is the
        // one it belongs in.
        if (this->pub.type != cipherTypeNone)
            this->pub.pass = pckReadBinP(packRead);
    }
    OBJ_NEW_END();

    FUNCTION_TEST_RETURN(CIPHER_SPEC, this);
}

/**********************************************************************************************************************************/
FN_EXTERN void
cipherSpecPack(PackWrite *const packWrite, const CipherSpec *const this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PACK_WRITE, packWrite);
        FUNCTION_TEST_PARAM(CIPHER_SPEC, this);
    FUNCTION_TEST_END();

    ASSERT(packWrite != NULL);
    ASSERT(this != NULL);

    pckWriteStrIdP(packWrite, cipherSpecType(this));

    // Nothing else is needed when there is no cipher
    if (cipherSpecType(this) != cipherTypeNone)
        pckWriteBinP(packWrite, cipherSpecPass(this));

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
FN_EXTERN void
cipherSpecToLog(const CipherSpec *const this, StringStatic *const debugLog)
{
    char typeZ[STRID_MAX + 1];
    strIdToZ(cipherSpecType(this), typeZ);

    // The pass is never logged
    strStcFmt(debugLog, "{type: %s}", typeZ);
}
