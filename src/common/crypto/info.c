/***********************************************************************************************************************************
Cipher Info
***********************************************************************************************************************************/
#include <build.h>

#include "common/crypto/info.h"
#include "common/encode.h"
#include "common/debug.h"
#include "common/log.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct CipherInfo
{
    CipherInfoPub pub;                                              // Publicly accessible variables
};

/**********************************************************************************************************************************/
FN_EXTERN CipherInfo *
cipherInfoNewNone(void)
{
    FUNCTION_TEST_VOID();

    OBJ_NEW_BEGIN(CipherInfo, .childQty = MEM_CONTEXT_QTY_MAX)
    {
        *this = (CipherInfo){.pub = {.type = cipherTypeNone}};
    }
    OBJ_NEW_END();

    FUNCTION_TEST_RETURN(CIPHER_INFO, this);
}

/**********************************************************************************************************************************/
FN_EXTERN CipherInfo *
cipherInfoNew(const CipherType type, const Buffer *const pass, const CipherInfoNewParam param)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING_ID, type);
        FUNCTION_TEST_PARAM(BUFFER, pass);
        FUNCTION_TEST_PARAM(STRING_ID, param.digest);
    FUNCTION_TEST_END();

    ASSERT(type == cipherTypeNone || (pass != NULL && !bufEmpty(pass)));

    OBJ_NEW_BEGIN(CipherInfo, .childQty = MEM_CONTEXT_QTY_MAX)
    {
        *this = (CipherInfo){.pub = {.type = type}};

        if (this->pub.type != cipherTypeNone)
        {
            if (param.digest == 0)
                this->pub.digest = hashTypeSha1;
            else
                this->pub.digest = param.digest;

            this->pub.pass = bufDup(pass);
        }
    }
    OBJ_NEW_END();

    FUNCTION_TEST_RETURN(CIPHER_INFO, this);
}

/**********************************************************************************************************************************/
FN_EXTERN CipherInfo *
cipherInfoNewStore(const CipherType type, const String *const pass)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING_ID, type);
        FUNCTION_TEST_PARAM(STRING, pass);                          // Use FUNCTION_TEST so pass is not logged
    FUNCTION_TEST_END();

    ASSERT((type == cipherTypeNone && pass == NULL) || (type != cipherTypeNone && pass != NULL));

    CipherInfo *result;

    if (type == cipherTypeNone)
        result = cipherInfoNewNone();
    else
        result = cipherInfoNewP(type, BUFSTR(pass));

    FUNCTION_TEST_RETURN(CIPHER_INFO, result);
}

/**********************************************************************************************************************************/
FN_EXTERN CipherInfo *
cipherInfoNewPack(PackRead *const packRead)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PACK_READ, packRead);
    FUNCTION_TEST_END();

    ASSERT(packRead != NULL);

    OBJ_NEW_BEGIN(CipherInfo, .childQty = MEM_CONTEXT_QTY_MAX)
    {
        *this = (CipherInfo){.pub = {.type = (CipherType)pckReadStrIdP(packRead)}};

        // Nothing else was written when there is no cipher. The pass is read here rather than copied in since this context is the
        // one it belongs in.
        if (this->pub.type != cipherTypeNone)
        {
            this->pub.digest = (HashType)pckReadStrIdP(packRead);
            this->pub.pass = pckReadBinP(packRead);
        }
    }
    OBJ_NEW_END();

    FUNCTION_TEST_RETURN(CIPHER_INFO, this);
}

/**********************************************************************************************************************************/
FN_EXTERN CipherInfo *
cipherInfoDup(const CipherInfo *const this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(CIPHER_INFO, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    CipherInfo *result;

    if (cipherInfoType(this) == cipherTypeNone)
        result = cipherInfoNewNone();
    else
        result = cipherInfoNewP(cipherInfoType(this), cipherInfoPass(this));

    FUNCTION_TEST_RETURN(CIPHER_INFO, result);
}

/**********************************************************************************************************************************/
FN_EXTERN void
cipherInfoPack(PackWrite *const packWrite, const CipherInfo *const this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PACK_WRITE, packWrite);
        FUNCTION_TEST_PARAM(CIPHER_INFO, this);
    FUNCTION_TEST_END();

    ASSERT(packWrite != NULL);
    ASSERT(this != NULL);

    pckWriteStrIdP(packWrite, cipherInfoType(this));

    // Nothing else is needed when there is no cipher
    if (cipherInfoType(this) != cipherTypeNone)
    {
        pckWriteStrIdP(packWrite, cipherInfoDigest(this));
        pckWriteBinP(packWrite, cipherInfoPass(this));
    }

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
FN_EXTERN void
cipherInfoToLog(const CipherInfo *const this, StringStatic *const debugLog)
{
    char typeZ[STRID_MAX + 1];
    strIdToZ(cipherInfoType(this), typeZ);

    // There is no digest when there is no cipher. The pass is never logged.
    if (cipherInfoType(this) == cipherTypeNone)
        strStcFmt(debugLog, "{type: %s}", typeZ);
    else
    {
        char digestZ[STRID_MAX + 1];
        strIdToZ(cipherInfoDigest(this), digestZ);

        strStcFmt(debugLog, "{type: %s, digest: %s}", typeZ, digestZ);
    }
}
