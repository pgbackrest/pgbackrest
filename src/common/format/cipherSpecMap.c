/***********************************************************************************************************************************
Cipher Spec Map
***********************************************************************************************************************************/
#include <build.h>

#include "common/debug.h"
#include "common/format/cipherSpecMap.h"
#include "common/log.h"

STRING_EXTERN(CIPHER_SPEC_MAP_ID_DEFAULT_STR,                      CIPHER_SPEC_MAP_ID_DEFAULT);

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct CipherSpecMap
{
    CipherSpecMapPub pub;                                           // Publicly accessible variables
};

/**********************************************************************************************************************************/
FN_EXTERN CipherSpecMap *
cipherSpecMapNew(void)
{
    FUNCTION_TEST_VOID();

    OBJ_NEW_BEGIN(CipherSpecMap, .childQty = MEM_CONTEXT_QTY_MAX)
    {
        *this = (CipherSpecMap)
        {
            .pub = {.list = lstNewP(sizeof(CipherSpecMapItem), .comparator = lstComparatorStr)},
        };
    }
    OBJ_NEW_END();

    FUNCTION_TEST_RETURN(CIPHER_SPEC_MAP, this);
}

/**********************************************************************************************************************************/
FN_EXTERN CipherSpecMap *
cipherSpecMapNewPack(PackRead *const packRead)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PACK_READ, packRead);
    FUNCTION_TEST_END();

    ASSERT(packRead != NULL);

    CipherSpecMap *const this = cipherSpecMapNew();

    MEM_CONTEXT_OBJ_BEGIN(this)
    {
        const unsigned int total = pckReadU32P(packRead);

        for (unsigned int keyIdx = 0; keyIdx < total; keyIdx++)
        {
            const CipherSpecMapItem item =
            {
                .id = pckReadStrP(packRead),
                .cipherSpec = cipherSpecNewPack(packRead),
            };

            lstAdd(this->pub.list, &item);
        }

        lstSort(this->pub.list, sortOrderAsc);

        this->pub.idCurrent = pckReadStrP(packRead);
    }
    MEM_CONTEXT_OBJ_END();

    // The key with no id is never current
    ASSERT(this->pub.idCurrent == NULL || !strEqZ(this->pub.idCurrent, CIPHER_SPEC_MAP_ID_DEFAULT));

    FUNCTION_TEST_RETURN(CIPHER_SPEC_MAP, this);
}

/**********************************************************************************************************************************/
FN_EXTERN const CipherSpec *
cipherSpecMapGet(const CipherSpecMap *const this, const String *const id)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(CIPHER_SPEC_MAP, this);
        FUNCTION_TEST_PARAM(STRING, id);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(id != NULL);

    const CipherSpecMapItem *const item = lstFind(this->pub.list, &id);

    // Error when request key was not found
    if (item == NULL)
        THROW_FMT(CryptoError, "unable to find cipher key '%s'", strZ(id));

    FUNCTION_TEST_RETURN_CONST(CIPHER_SPEC, item->cipherSpec);
}

/**********************************************************************************************************************************/
FN_EXTERN void
cipherSpecMapAdd(CipherSpecMap *const this, const String *const id, const CipherSpec *const cipherSpec)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(CIPHER_SPEC_MAP, this);
        FUNCTION_TEST_PARAM(STRING, id);
        FUNCTION_TEST_PARAM(CIPHER_SPEC, cipherSpec);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(id != NULL);
    ASSERT(!strEmpty(id));
    ASSERT(cipherSpec != NULL);
    ASSERT(lstFind(this->pub.list, &id) == NULL);

    MEM_CONTEXT_OBJ_BEGIN(this)
    {
        const CipherSpecMapItem item = {.id = strDup(id), .cipherSpec = cipherSpecDupP(cipherSpec)};

        lstAdd(this->pub.list, &item);
        lstSort(this->pub.list, sortOrderAsc);

        // The default key id is for files that contain no id so it is never current
        if (!strEqZ(id, CIPHER_SPEC_MAP_ID_DEFAULT))
            this->pub.idCurrent = item.id;
    }
    MEM_CONTEXT_OBJ_END();

    // The key with no id is never current and every other key is current once added
    ASSERT(strEqZ(id, CIPHER_SPEC_MAP_ID_DEFAULT) || strEq(cipherSpecMapIdCurrent(this), id));

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
FN_EXTERN void
cipherSpecMapIdCurrentSet(CipherSpecMap *const this, const String *const id)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(CIPHER_SPEC_MAP, this);
        FUNCTION_TEST_PARAM(STRING, id);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(id != NULL);
    ASSERT(!strEqZ(id, CIPHER_SPEC_MAP_ID_DEFAULT));

    const CipherSpecMapItem *const item = lstFind(this->pub.list, &id);
    ASSERT(item != NULL);

    // Point at the id stored with the key rather than duplicating it
    this->pub.idCurrent = item->id;

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
FN_EXTERN CipherSpecMap *
cipherSpecMapDup(const CipherSpecMap *const this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(CIPHER_SPEC_MAP, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    CipherSpecMap *const result = cipherSpecMapNew();

    for (unsigned int keyIdx = 0; keyIdx < cipherSpecMapSize(this); keyIdx++)
    {
        const CipherSpecMapItem *const item = cipherSpecMapGetIdx(this, keyIdx);

        cipherSpecMapAdd(result, item->id, item->cipherSpec);
    }

    if (cipherSpecMapIdCurrent(this) != NULL)
        cipherSpecMapIdCurrentSet(result, cipherSpecMapIdCurrent(this));

    FUNCTION_TEST_RETURN(CIPHER_SPEC_MAP, result);
}

/**********************************************************************************************************************************/
FN_EXTERN void
cipherSpecMapPack(PackWrite *const packWrite, const CipherSpecMap *const this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PACK_WRITE, packWrite);
        FUNCTION_TEST_PARAM(CIPHER_SPEC_MAP, this);
    FUNCTION_TEST_END();

    ASSERT(packWrite != NULL);
    ASSERT(this != NULL);

    pckWriteU32P(packWrite, cipherSpecMapSize(this));

    for (unsigned int keyIdx = 0; keyIdx < cipherSpecMapSize(this); keyIdx++)
    {
        const CipherSpecMapItem *const item = cipherSpecMapGetIdx(this, keyIdx);

        pckWriteStrP(packWrite, item->id);
        cipherSpecPack(packWrite, item->cipherSpec);
    }

    pckWriteStrP(packWrite, cipherSpecMapIdCurrent(this));

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
FN_EXTERN void
cipherSpecMapToLog(const CipherSpecMap *const this, StringStatic *const debugLog)
{
    // List only the ids (never the keys themselves)
    strStcCat(debugLog, "{ids: [");

    for (unsigned int keyIdx = 0; keyIdx < cipherSpecMapSize(this); keyIdx++)
    {
        if (keyIdx != 0)
            strStcCat(debugLog, ", ");

        strStcFmt(debugLog, "%s", strZ(cipherSpecMapGetIdx(this, keyIdx)->id));
    }

    strStcCat(debugLog, "]}");
}
