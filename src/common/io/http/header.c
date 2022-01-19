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
HttpHeader *
httpHeaderNew(const StringList *redactList)
{
    FUNCTION_TEST_VOID();

    HttpHeader *this = NULL;

    OBJ_NEW_BEGIN(HttpHeader)
    {
        // Allocate state and set context
        this = OBJ_NEW_ALLOC();

        *this = (HttpHeader)
        {
            .redactList = strLstDup(redactList),
            .kv = kvNew(),
        };
    }
    OBJ_NEW_END();

    FUNCTION_TEST_RETURN(this);
}

/**********************************************************************************************************************************/
HttpHeader *
httpHeaderDup(const HttpHeader *header, const StringList *redactList)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(HTTP_HEADER, header);
        FUNCTION_TEST_PARAM(STRING_LIST, redactList);
    FUNCTION_TEST_END();

    HttpHeader *this = NULL;

    if (header != NULL)
    {
        OBJ_NEW_BEGIN(HttpHeader)
        {
            // Allocate state and set context
            this = OBJ_NEW_ALLOC();

            *this = (HttpHeader)
            {
                .redactList = redactList == NULL ? strLstDup(header->redactList) : strLstDup(redactList),
                .kv = kvDup(header->kv),
            };
        }
        OBJ_NEW_END();
    }

    FUNCTION_TEST_RETURN(this);
}

/**********************************************************************************************************************************/
HttpHeader *
httpHeaderAdd(HttpHeader *this, const String *key, const String *value)
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
    const Variant *keyVar = VARSTR(key);
    const Variant *valueVar = kvGet(this->kv, keyVar);

    // If the key exists then append the new value.  The HTTP spec (RFC 2616, Section 4.2) says that if a header appears more than
    // once then it is equivalent to a single comma-separated header.  There appear to be a few exceptions such as Set-Cookie, but
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

    FUNCTION_TEST_RETURN(this);
}

/**********************************************************************************************************************************/
const String *
httpHeaderGet(const HttpHeader *this, const String *key)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(HTTP_HEADER, this);
        FUNCTION_TEST_PARAM(STRING, key);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(key != NULL);

    FUNCTION_TEST_RETURN(varStr(kvGet(this->kv, VARSTR(key))));
}

/**********************************************************************************************************************************/
StringList *
httpHeaderList(const HttpHeader *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(HTTP_HEADER, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(strLstSort(strLstNewVarLst(kvKeyList(this->kv)), sortOrderAsc));
}

/**********************************************************************************************************************************/
HttpHeader *
httpHeaderPut(HttpHeader *this, const String *key, const String *value)
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

    FUNCTION_TEST_RETURN(this);
}

/**********************************************************************************************************************************/
HttpHeader *
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
    }

    FUNCTION_TEST_RETURN(this);
}

/**********************************************************************************************************************************/
bool
httpHeaderRedact(const HttpHeader *this, const String *key)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(HTTP_HEADER, this);
        FUNCTION_TEST_PARAM(STRING, key);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(key != NULL);

    FUNCTION_TEST_RETURN(this->redactList != NULL && strLstExists(this->redactList, key));
}

/**********************************************************************************************************************************/
String *
httpHeaderToLog(const HttpHeader *this)
{
    String *result = strCatZ(strNew(), "{");
    const StringList *keyList = httpHeaderList(this);

    for (unsigned int keyIdx = 0; keyIdx < strLstSize(keyList); keyIdx++)
    {
        const String *key = strLstGet(keyList, keyIdx);

        if (strSize(result) != 1)
            strCatZ(result, ", ");

        if (httpHeaderRedact(this, key))
            strCatFmt(result, "%s: <redacted>", strZ(key));
        else
            strCatFmt(result, "%s: '%s'", strZ(key), strZ(httpHeaderGet(this, key)));
    }

    strCatZ(result, "}");

    return result;
}
