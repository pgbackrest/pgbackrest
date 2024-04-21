/***********************************************************************************************************************************
HTTP Query
***********************************************************************************************************************************/
#include "build.auto.h"

#include "common/debug.h"
#include "common/io/http/common.h"
#include "common/io/http/query.h"
#include "common/type/keyValue.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct HttpQuery
{
    KeyValue *kv;                                                   // KeyValue store
    const StringList *redactList;                                   // List of keys to redact values for
};

/**********************************************************************************************************************************/
FN_EXTERN HttpQuery *
httpQueryNew(const HttpQueryNewParam param)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(KEY_VALUE, param.kv);
        FUNCTION_TEST_PARAM(STRING_LIST, param.redactList);
    FUNCTION_TEST_END();

    OBJ_NEW_BEGIN(HttpQuery, .childQty = MEM_CONTEXT_QTY_MAX)
    {
        *this = (HttpQuery)
        {
            .kv = param.kv != NULL ? kvDup(param.kv) : kvNew(),
            .redactList = strLstDup(param.redactList),
        };
    }
    OBJ_NEW_END();

    FUNCTION_TEST_RETURN(HTTP_QUERY, this);
}

/**********************************************************************************************************************************/
FN_EXTERN HttpQuery *
httpQueryNewStr(const String *query)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, query);
    FUNCTION_TEST_END();

    ASSERT(query != NULL);

    OBJ_NEW_BEGIN(HttpQuery, .childQty = MEM_CONTEXT_QTY_MAX)
    {
        *this = (HttpQuery)
        {
            .kv = kvNew(),
        };

        MEM_CONTEXT_TEMP_BEGIN()
        {
            // Remove initial ? when present
            if (strBeginsWithZ(query, "?"))
                query = strSub(query, 1);

            // Split query into individual key value pairs
            const StringList *const keyValueList = strLstNewSplitZ(query, "&");

            for (unsigned int keyValueIdx = 0; keyValueIdx < strLstSize(keyValueList); keyValueIdx++)
            {
                // Add each key/value pair
                StringList *keyValue = strLstNewSplitZ(strLstGet(keyValueList, keyValueIdx), "=");

                if (strLstSize(keyValue) != 2)
                {
                    THROW_FMT(
                        FormatError, "invalid key/value '%s' in query '%s'", strZ(strLstGet(keyValueList, keyValueIdx)),
                        strZ(query));
                }

                httpQueryAdd(this, httpUriDecode(strLstGet(keyValue, 0)), httpUriDecode(strLstGet(keyValue, 1)));
            }
        }
        MEM_CONTEXT_TEMP_END();
    }
    OBJ_NEW_END();

    FUNCTION_TEST_RETURN(HTTP_QUERY, this);
}

/**********************************************************************************************************************************/
FN_EXTERN HttpQuery *
httpQueryDup(const HttpQuery *const query, const HttpQueryDupParam param)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(HTTP_QUERY, query);
        FUNCTION_TEST_PARAM(STRING_LIST, param.redactList);
    FUNCTION_TEST_END();

    if (query == NULL)
        FUNCTION_TEST_RETURN(HTTP_QUERY, NULL);

    OBJ_NEW_BEGIN(HttpQuery, .childQty = MEM_CONTEXT_QTY_MAX)
    {
        *this = (HttpQuery)
        {
            .kv = kvDup(query->kv),
            .redactList = param.redactList != NULL ? strLstDup(param.redactList) : strLstDup(query->redactList),
        };
    }
    OBJ_NEW_END();

    FUNCTION_TEST_RETURN(HTTP_QUERY, this);
}

/**********************************************************************************************************************************/
FN_EXTERN HttpQuery *
httpQueryAdd(HttpQuery *const this, const String *const key, const String *const value)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(HTTP_QUERY, this);
        FUNCTION_TEST_PARAM(STRING, key);
        FUNCTION_TEST_PARAM(STRING, value);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(key != NULL);
    ASSERT(value != NULL);

    // Make sure the key does not already exist
    const Variant *keyVar = VARSTR(key);

    if (kvGet(this->kv, keyVar) != NULL)
        THROW_FMT(AssertError, "key '%s' already exists", strZ(key));

    // Store the key
    kvPut(this->kv, keyVar, VARSTR(value));

    FUNCTION_TEST_RETURN(HTTP_QUERY, this);
}

/**********************************************************************************************************************************/
FN_EXTERN const String *
httpQueryGet(const HttpQuery *const this, const String *const key)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(HTTP_QUERY, this);
        FUNCTION_TEST_PARAM(STRING, key);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(key != NULL);

    FUNCTION_TEST_RETURN_CONST(STRING, varStr(kvGet(this->kv, VARSTR(key))));
}

/**********************************************************************************************************************************/
FN_EXTERN StringList *
httpQueryList(const HttpQuery *const this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(HTTP_QUERY, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(STRING_LIST, strLstSort(strLstNewVarLst(kvKeyList(this->kv)), sortOrderAsc));
}

/**********************************************************************************************************************************/
FN_EXTERN HttpQuery *
httpQueryMerge(HttpQuery *const this, const HttpQuery *const query)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(HTTP_QUERY, this);
        FUNCTION_TEST_PARAM(HTTP_QUERY, query);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(query != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        const VariantList *const keyList = kvKeyList(query->kv);

        for (unsigned int keyIdx = 0; keyIdx < varLstSize(keyList); keyIdx++)
        {
            const Variant *const key = varLstGet(keyList, keyIdx);

            httpQueryAdd(this, varStr(key), varStr(kvGet(query->kv, key)));
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_TEST_RETURN(HTTP_QUERY, this);
}

/**********************************************************************************************************************************/
FN_EXTERN HttpQuery *
httpQueryPut(HttpQuery *const this, const String *const key, const String *const value)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(HTTP_QUERY, this);
        FUNCTION_TEST_PARAM(STRING, key);
        FUNCTION_TEST_PARAM(STRING, value);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(key != NULL);
    ASSERT(value != NULL);

    // Store the key
    kvPut(this->kv, VARSTR(key), VARSTR(value));

    FUNCTION_TEST_RETURN(HTTP_QUERY, this);
}

/**********************************************************************************************************************************/
FN_EXTERN bool
httpQueryRedact(const HttpQuery *const this, const String *const key)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(HTTP_QUERY, this);
        FUNCTION_TEST_PARAM(STRING, key);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(key != NULL);

    FUNCTION_TEST_RETURN(BOOL, this->redactList != NULL && strLstExists(this->redactList, key));
}

/**********************************************************************************************************************************/
FN_EXTERN String *
httpQueryRender(const HttpQuery *const this, const HttpQueryRenderParam param)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(HTTP_QUERY, this);
        FUNCTION_TEST_PARAM(BOOL, param.redact);
    FUNCTION_TEST_END();

    String *result = NULL;

    if (this != NULL)
    {
        MEM_CONTEXT_TEMP_BEGIN()
        {
            const StringList *const keyList = httpQueryList(this);

            if (!strLstEmpty(keyList))
            {
                MEM_CONTEXT_PRIOR_BEGIN()
                {
                    result = strNew();
                }
                MEM_CONTEXT_PRIOR_END();

                for (unsigned int keyIdx = 0; keyIdx < strLstSize(keyList); keyIdx++)
                {
                    const String *const key = strLstGet(keyList, keyIdx);

                    if (strSize(result) != 0)
                        strCatZ(result, "&");

                    strCatFmt(
                        result, "%s=%s", strZ(httpUriEncode(key, false)),
                        param.redact && httpQueryRedact(this, key) ?
                            "<redacted>" : strZ(httpUriEncode(httpQueryGet(this, key), false)));
                }
            }
        }
        MEM_CONTEXT_TEMP_END();
    }

    FUNCTION_TEST_RETURN(STRING, result);
}

/**********************************************************************************************************************************/
FN_EXTERN void
httpQueryToLog(const HttpQuery *const this, StringStatic *const debugLog)
{
    const VariantList *const keyList = kvKeyList(this->kv);

    strStcCatChr(debugLog, '{');

    for (unsigned int keyIdx = 0; keyIdx < varLstSize(keyList); keyIdx++)
    {
        const String *const key = varStr(varLstGet(keyList, keyIdx));

        if (keyIdx != 0)
            strStcCat(debugLog, ", ");

        if (httpQueryRedact(this, key))
            strStcFmt(debugLog, "%s: <redacted>", strZ(key));
        else
            strStcFmt(debugLog, "%s: '%s'", strZ(key), strZ(httpQueryGet(this, key)));
    }

    strStcCatChr(debugLog, '}');
}
