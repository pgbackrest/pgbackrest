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
cipherSpecNew(const CipherType type, const Buffer *const pass, const CipherSpecNewParam param)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING_ID, type);
        FUNCTION_TEST_PARAM(BUFFER, pass);
        FUNCTION_TEST_PARAM(STRING_ID, param.digest);
    FUNCTION_TEST_END();

    ASSERT((type == cipherTypeNone) == (pass == NULL));
    ASSERT(pass == NULL || !bufEmpty(pass));

    OBJ_NEW_BEGIN(CipherSpec, .childQty = MEM_CONTEXT_QTY_MAX)
    {
        *this = (CipherSpec){.pub = {.type = type}};

        if (this->pub.type != cipherTypeNone)
        {
            if (param.digest == 0)
                this->pub.digest = hashTypeSha256;
            else
                this->pub.digest = param.digest;

            this->pub.pass = bufDup(pass);
        }
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
        {
            this->pub.digest = (HashType)pckReadStrIdP(packRead);
            this->pub.pass = pckReadBinP(packRead);
        }
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
    {
        pckWriteStrIdP(packWrite, cipherSpecDigest(this));
        pckWriteBinP(packWrite, cipherSpecPass(this));
    }

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
FN_EXTERN void
cipherSpecToLog(const CipherSpec *const this, StringStatic *const debugLog)
{
    char typeZ[STRID_MAX + 1];
    strIdToZ(cipherSpecType(this), typeZ);

    // There is no digest when there is no cipher. The pass is never logged.
    if (cipherSpecType(this) == cipherTypeNone)
        strStcFmt(debugLog, "{type: %s}", typeZ);
    else
    {
        char digestZ[STRID_MAX + 1];
        strIdToZ(cipherSpecDigest(this), digestZ);

        strStcFmt(debugLog, "{type: %s, digest: %s}", typeZ, digestZ);
    }
}
