/***********************************************************************************************************************************
Http Header
***********************************************************************************************************************************/
#include "common/debug.h"
#include "common/io/http/header.h"
#include "common/memContext.h"
#include "common/type/keyValue.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct HttpHeader
{
    MemContext *memContext;                                         // Mem context
    const StringList *redactList;                                   // List of headers to redact during logging
    KeyValue *kv;                                                   // KeyValue store
};

/***********************************************************************************************************************************
New object
***********************************************************************************************************************************/
HttpHeader *
httpHeaderNew(const StringList *redactList)
{
    FUNCTION_TEST_VOID();

    HttpHeader *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("HttpHeader")
    {
        // Allocate state and set context
        this = memNew(sizeof(HttpHeader));
        this->memContext = MEM_CONTEXT_NEW();

        this->redactList = redactList;
        this->kv = kvNew();
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_TEST_RETURN(HTTP_HEADER, this);
}

/***********************************************************************************************************************************
Add a header
***********************************************************************************************************************************/
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

    MEM_CONTEXT_BEGIN(this->memContext)
    {
        // Make sure the key does not already exist
        Variant *keyVar = varNewStr(key);

        if (kvGet(this->kv, keyVar) != NULL)
            THROW_FMT(AssertError, "key '%s' already exists", strPtr(key));

        // Store the key
        kvPut(this->kv, keyVar, varNewStr(value));
    }
    MEM_CONTEXT_END();

    FUNCTION_TEST_RETURN(HTTP_HEADER, this);
}

/***********************************************************************************************************************************
Get a value using the key
***********************************************************************************************************************************/
const String *
httpHeaderGet(const HttpHeader *this, const String *key)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(HTTP_HEADER, this);
        FUNCTION_TEST_PARAM(STRING, key);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(key != NULL);

    String *result = NULL;

    MEM_CONTEXT_BEGIN(this->memContext)
    {
        result = varStr(kvGet(this->kv, varNewStr(key)));
    }
    MEM_CONTEXT_END();

    FUNCTION_TEST_RETURN(STRING, result);
}

/***********************************************************************************************************************************
Get list of keys
***********************************************************************************************************************************/
StringList *
httpHeaderList(const HttpHeader *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(HTTP_HEADER, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(STRING_LIST, strLstSort(strLstNewVarLst(kvKeyList(this->kv)), sortOrderAsc));
}

/***********************************************************************************************************************************
Move object to a new mem context
***********************************************************************************************************************************/
HttpHeader *
httpHeaderMove(HttpHeader *this, MemContext *parentNew)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(HTTP_HEADER, this);
        FUNCTION_TEST_PARAM(MEM_CONTEXT, parentNew);
    FUNCTION_TEST_END();

    ASSERT(parentNew != NULL);

    if (this != NULL)
        memContextMove(this->memContext, parentNew);

    FUNCTION_TEST_RETURN(HTTP_HEADER, this);
}

/***********************************************************************************************************************************
Put a header
***********************************************************************************************************************************/
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

    MEM_CONTEXT_BEGIN(this->memContext)
    {
        // Store the key
        kvPut(this->kv, varNewStr(key), varNewStr(value));
    }
    MEM_CONTEXT_END();

    FUNCTION_TEST_RETURN(HTTP_HEADER, this);
}

/***********************************************************************************************************************************
Should the header be redacted when logging?
***********************************************************************************************************************************/
bool
httpHeaderRedact(const HttpHeader *this, const String *key)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(HTTP_HEADER, this);
        FUNCTION_TEST_PARAM(STRING, key);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(key != NULL);

    FUNCTION_TEST_RETURN(BOOL, this->redactList != NULL && strLstExists(this->redactList, key));
}

/***********************************************************************************************************************************
Render as string for logging
***********************************************************************************************************************************/
String *
httpHeaderToLog(const HttpHeader *this)
{
    String *result = strNew("{");
    const StringList *keyList = httpHeaderList(this);

    for (unsigned int keyIdx = 0; keyIdx < strLstSize(keyList); keyIdx++)
    {
        const String *key = strLstGet(keyList, keyIdx);

        if (strSize(result) != 1)
            strCat(result, ", ");

        if (httpHeaderRedact(this, key))
            strCatFmt(result, "%s: <redacted>", strPtr(key));
        else
            strCatFmt(result, "%s: '%s'", strPtr(key), strPtr(httpHeaderGet(this, key)));
    }

    strCat(result, "}");

    return result;
}

/***********************************************************************************************************************************
Free the object
***********************************************************************************************************************************/
void
httpHeaderFree(HttpHeader *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(HTTP_HEADER, this);
    FUNCTION_TEST_END();

    if (this != NULL)
        memContextFree(this->memContext);

    FUNCTION_TEST_RETURN_VOID();
}
