/***********************************************************************************************************************************
HTTP Header
***********************************************************************************************************************************/
#include "build.auto.h"

#include "common/debug.h"
#include "common/io/http/header.h"
#include "common/io/http/request.h"
#include "common/type/keyValue.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct HttpHeader
{
    const StringList *redactList;                                   // List of headers to redact during logging
    KeyValue *kv;                                                   // KeyValue store
};

/**********************************************************************************************************************************/
FN_EXTERN HttpHeader *
httpHeaderNew(const StringList *const redactList)
{
    FUNCTION_TEST_VOID();

    OBJ_NEW_BEGIN(HttpHeader, .childQty = MEM_CONTEXT_QTY_MAX)
    {
        *this = (HttpHeader)
        {
            .redactList = strLstDup(redactList),
            .kv = kvNew(),
        };
    }
    OBJ_NEW_END();

    FUNCTION_TEST_RETURN(HTTP_HEADER, this);
}

/**********************************************************************************************************************************/
FN_EXTERN HttpHeader *
httpHeaderDup(const HttpHeader *const header, const StringList *const redactList)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(HTTP_HEADER, header);
        FUNCTION_TEST_PARAM(STRING_LIST, redactList);
    FUNCTION_TEST_END();

    if (header == NULL)
        FUNCTION_TEST_RETURN(HTTP_HEADER, NULL);

    OBJ_NEW_BEGIN(HttpHeader, .childQty = MEM_CONTEXT_QTY_MAX)
    {
        *this = (HttpHeader)
        {
            .redactList = redactList == NULL ? strLstDup(header->redactList) : strLstDup(redactList),
            .kv = kvDup(header->kv),
        };
    }
    OBJ_NEW_END();

    FUNCTION_TEST_RETURN(HTTP_HEADER, this);
}

/**********************************************************************************************************************************/
FN_EXTERN HttpHeader *
httpHeaderAdd(HttpHeader *const this, const String *const key, const String *const value)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(HTTP_HEADER, this);
        FUNCTION_TEST_PARAM(STRING, key);
        FUNCTION_TEST_PARAM(STRING, value);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(key != NULL);
    ASSERT(value != NULL);

    // Check if the key already exists
    const Variant *const keyVar = VARSTR(key);
    const Variant *const valueVar = kvGet(this->kv, keyVar);

    // If the key exists then append the new value. The HTTP spec (RFC 2616, Section 4.2) says that if a header appears more than
    // once then it is equivalent to a single comma-separated header. There appear to be a few exceptions such as Set-Cookie, but
    // they should not be of concern to us here.
    if (valueVar != NULL)
    {
        MEM_CONTEXT_TEMP_BEGIN()
        {
            String *const valueAppend = strCat(strNew(), varStr(valueVar));
            strCatZ(valueAppend, ", ");
            strCatZ(valueAppend, strZ(value));

            kvPut(this->kv, keyVar, VARSTR(valueAppend));
        }
        MEM_CONTEXT_TEMP_END();
    }
    // Else store the key
    else
        kvPut(this->kv, keyVar, VARSTR(value));

    FUNCTION_TEST_RETURN(HTTP_HEADER, this);
}

/**********************************************************************************************************************************/
FN_EXTERN const String *
httpHeaderGet(const HttpHeader *const this, const String *const key)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(HTTP_HEADER, this);
        FUNCTION_TEST_PARAM(STRING, key);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(key != NULL);

    FUNCTION_TEST_RETURN_CONST(STRING, varStr(kvGet(this->kv, VARSTR(key))));
}

/**********************************************************************************************************************************/
FN_EXTERN StringList *
httpHeaderList(const HttpHeader *const this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(HTTP_HEADER, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(STRING_LIST, strLstSort(strLstNewVarLst(kvKeyList(this->kv)), sortOrderAsc));
}

/**********************************************************************************************************************************/
FN_EXTERN HttpHeader *
httpHeaderPut(HttpHeader *const this, const String *const key, const String *const value)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(HTTP_HEADER, this);
        FUNCTION_TEST_PARAM(STRING, key);
        FUNCTION_TEST_PARAM(STRING, value);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(key != NULL);
    ASSERT(value != NULL);

    // Store the key
    kvPut(this->kv, VARSTR(key), VARSTR(value));

    FUNCTION_TEST_RETURN(HTTP_HEADER, this);
}

/**********************************************************************************************************************************/
FN_EXTERN HttpHeader *
httpHeaderPutRange(HttpHeader *const this, const uint64_t offset, const Variant *const limit)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(HTTP_HEADER, this);
        FUNCTION_TEST_PARAM(UINT64, offset);
        FUNCTION_TEST_PARAM(VARIANT, limit);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(limit == NULL || varType(limit) == varTypeUInt64);

    if (offset != 0 || limit != NULL)
    {
        String *const range = strCatFmt(strNew(), HTTP_HEADER_RANGE_BYTES "=%" PRIu64 "-", offset);

        if (limit != NULL)
            strCatFmt(range, "%" PRIu64, offset + varUInt64(limit) - 1);

        httpHeaderPut(this, HTTP_HEADER_RANGE_STR, range);
        strFree(range);
    }

    FUNCTION_TEST_RETURN(HTTP_HEADER, this);
}

/**********************************************************************************************************************************/
FN_EXTERN bool
httpHeaderRedact(const HttpHeader *const this, const String *const key)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(HTTP_HEADER, this);
        FUNCTION_TEST_PARAM(STRING, key);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(key != NULL);

    FUNCTION_TEST_RETURN(BOOL, this->redactList != NULL && strLstExists(this->redactList, key));
}

/**********************************************************************************************************************************/
FN_EXTERN void
httpHeaderToLog(const HttpHeader *const this, StringStatic *const debugLog)
{
    const VariantList *const keyList = kvKeyList(this->kv);

    strStcCatChr(debugLog, '{');

    for (unsigned int keyIdx = 0; keyIdx < varLstSize(keyList); keyIdx++)
    {
        const String *const key = varStr(varLstGet(keyList, keyIdx));

        if (keyIdx != 0)
            strStcCat(debugLog, ", ");

        if (httpHeaderRedact(this, key))
            strStcFmt(debugLog, "%s: <redacted>", strZ(key));
        else
            strStcFmt(debugLog, "%s: '%s'", strZ(key), strZ(httpHeaderGet(this, key)));
    }

    strStcCatChr(debugLog, '}');
}
