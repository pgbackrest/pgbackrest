/***********************************************************************************************************************************
HTTP Query
***********************************************************************************************************************************/
#include "build.auto.h"

#include "common/debug.h"
#include "common/io/http/common.h"
#include "common/io/http/query.h"
#include "common/memContext.h"
#include "common/type/keyValue.h"
#include "common/type/object.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct HttpQuery
{
    MemContext *memContext;                                         // Mem context
    KeyValue *kv;                                                   // KeyValue store
};

OBJECT_DEFINE_MOVE(HTTP_QUERY);
OBJECT_DEFINE_FREE(HTTP_QUERY);

/**********************************************************************************************************************************/
HttpQuery *
httpQueryNew(void)
{
    FUNCTION_TEST_VOID();

    HttpQuery *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("HttpQuery")
    {
        // Allocate state and set context
        this = memNew(sizeof(HttpQuery));

        *this = (HttpQuery)
        {
            .memContext = MEM_CONTEXT_NEW(),
            .kv = kvNew(),
        };
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_TEST_RETURN(this);
}

/**********************************************************************************************************************************/
HttpQuery *
httpQueryNewStr(const String *query)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, query);
    FUNCTION_TEST_END();

    HttpQuery *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("HttpQuery")
    {
        // Allocate state and set context
        this = memNew(sizeof(HttpQuery));

        *this = (HttpQuery)
        {
            .memContext = MEM_CONTEXT_NEW(),
            .kv = kvNew(),
        };

        MEM_CONTEXT_TEMP_BEGIN()
        {
            // Remove initial ? when present
            if (strBeginsWithZ(query, QUESTION_Z))
                query = strSub(query, 1);

            // Split query into individual key value pairs
            StringList *keyValueList = strLstNewSplitZ(query, AMPERSAND_Z);

            for (unsigned int keyValueIdx = 0; keyValueIdx < strLstSize(keyValueList); keyValueIdx++)
            {
                // Add each key/value pair
                StringList *keyValue = strLstNewSplitZ(strLstGet(keyValueList, keyValueIdx), EQ_Z);

                if (strLstSize(keyValue) != 2)
                {
                    THROW_FMT(
                        FormatError, "invalid key/value '%s' in query '%s'", strPtr(strLstGet(keyValueList, keyValueIdx)),
                        strPtr(query));
                }

                httpQueryAdd(this, httpUriDecode(strLstGet(keyValue, 0)), httpUriDecode(strLstGet(keyValue, 1)));
            }
        }
        MEM_CONTEXT_TEMP_END();
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_TEST_RETURN(this);
}

/**********************************************************************************************************************************/
HttpQuery *
httpQueryDup(const HttpQuery *query)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(HTTP_QUERY, query);
    FUNCTION_TEST_END();

    HttpQuery *this = NULL;

    if (query != NULL)
    {
        MEM_CONTEXT_NEW_BEGIN("HttpQuery")
        {
            // Allocate state and set context
            this = memNew(sizeof(HttpQuery));

            *this = (HttpQuery)
            {
                .memContext = MEM_CONTEXT_NEW(),
                .kv = kvDup(query->kv),
            };
        }
        MEM_CONTEXT_NEW_END();
    }

    FUNCTION_TEST_RETURN(this);
}

/**********************************************************************************************************************************/
HttpQuery *
httpQueryAdd(HttpQuery *this, const String *key, const String *value)
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
        THROW_FMT(AssertError, "key '%s' already exists", strPtr(key));

    // Store the key
    kvPut(this->kv, keyVar, VARSTR(value));

    FUNCTION_TEST_RETURN(this);
}

/**********************************************************************************************************************************/
const String *
httpQueryGet(const HttpQuery *this, const String *key)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(HTTP_QUERY, this);
        FUNCTION_TEST_PARAM(STRING, key);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(key != NULL);

    FUNCTION_TEST_RETURN(varStr(kvGet(this->kv, VARSTR(key))));
}

/**********************************************************************************************************************************/
StringList *
httpQueryList(const HttpQuery *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(HTTP_QUERY, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(strLstSort(strLstNewVarLst(kvKeyList(this->kv)), sortOrderAsc));
}

/**********************************************************************************************************************************/
HttpQuery *
httpQueryMerge(HttpQuery *this, const HttpQuery *query)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(HTTP_QUERY, this);
        FUNCTION_TEST_PARAM(HTTP_QUERY, query);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(query != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        const VariantList *keyList = kvKeyList(query->kv);

        for (unsigned int keyIdx = 0; keyIdx < varLstSize(keyList); keyIdx++)
        {
            const Variant *key = varLstGet(keyList, keyIdx);

            httpQueryAdd(this, varStr(key), varStr(kvGet(query->kv, key)));
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_TEST_RETURN(this);
}

/**********************************************************************************************************************************/
HttpQuery *
httpQueryPut(HttpQuery *this, const String *key, const String *value)
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

    FUNCTION_TEST_RETURN(this);
}

/**********************************************************************************************************************************/
String *
httpQueryRender(const HttpQuery *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(HTTP_QUERY, this);
    FUNCTION_TEST_END();

    String *result = NULL;

    if (this != NULL)
    {
        const StringList *keyList = httpQueryList(this);

        if (strLstSize(keyList) > 0)
        {
            result = strNew("");

            MEM_CONTEXT_TEMP_BEGIN()
            {
                for (unsigned int keyIdx = 0; keyIdx < strLstSize(keyList); keyIdx++)
                {
                    if (strSize(result) != 0)
                        strCatZ(result, "&");

                    strCatFmt(
                        result, "%s=%s", strPtr(strLstGet(keyList, keyIdx)),
                        strPtr(httpUriEncode(httpQueryGet(this, strLstGet(keyList, keyIdx)), false)));
                }
            }
            MEM_CONTEXT_TEMP_END();
        }
    }

    FUNCTION_TEST_RETURN(result);
}

/**********************************************************************************************************************************/
String *
httpQueryToLog(const HttpQuery *this)
{
    String *result = strNew("{");
    const StringList *keyList = httpQueryList(this);

    for (unsigned int keyIdx = 0; keyIdx < strLstSize(keyList); keyIdx++)
    {
        if (strSize(result) != 1)
            strCatZ(result, ", ");

        strCatFmt(
            result, "%s: '%s'", strPtr(strLstGet(keyList, keyIdx)),
            strPtr(httpQueryGet(this, strLstGet(keyList, keyIdx))));
    }

    strCatZ(result, "}");

    return result;
}
