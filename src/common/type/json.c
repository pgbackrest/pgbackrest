/***********************************************************************************************************************************
Convert JSON to/from KeyValue
***********************************************************************************************************************************/
#include <ctype.h>

#include <stdio.h> // cshang

#include "common/debug.h"
#include "common/log.h"
#include "common/type/json.h"

/***********************************************************************************************************************************
Internal helper functions
***********************************************************************************************************************************/
static Variant *
jsonString(const char *jsonC, size_t strSize, unsigned int *jsonPos, unsigned int *valueBeginPos)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(CHARPY, jsonC);
        FUNCTION_DEBUG_PARAM(SIZE, strSize);
        FUNCTION_DEBUG_PARAM(UINTP, jsonPos);
        FUNCTION_DEBUG_PARAM(UINTP, valueBeginPos);
    FUNCTION_DEBUG_END();

    *valueBeginPos = *valueBeginPos + 1;
    *jsonPos = *jsonPos + 1;

    while (jsonC[*jsonPos] != '"' && *jsonPos < strSize - 1)
        *jsonPos = *jsonPos + 1;

    if (jsonC[*jsonPos] != '"')
        THROW_FMT(JsonFormatError, "expected '\"' but found '%c'", jsonC[*jsonPos]);

    Variant *result = varNewStr(strNewN(jsonC + *valueBeginPos, *jsonPos - *valueBeginPos));

    *jsonPos = *jsonPos + 1;

    FUNCTION_TEST_RESULT(VARIANT, result);
}

static Variant *
jsonNumeric(const char *jsonC, size_t strSize, unsigned int *jsonPos, unsigned int *valueBeginPos)
{
        FUNCTION_DEBUG_BEGIN(logLevelTrace);
            FUNCTION_DEBUG_PARAM(CHARPY, jsonC);
            FUNCTION_DEBUG_PARAM(SIZE, strSize);
            FUNCTION_DEBUG_PARAM(UINTP, jsonPos);
            FUNCTION_DEBUG_PARAM(UINTP, valueBeginPos);
        FUNCTION_DEBUG_END();

    while (isdigit(jsonC[*jsonPos]) && *jsonPos < strSize - 1)
        *jsonPos = *jsonPos + 1;

    Variant *result = varNewUInt64(cvtZToUInt64(strPtr(strNewN(jsonC + *valueBeginPos, *jsonPos - *valueBeginPos))));

    FUNCTION_TEST_RESULT(VARIANT, result);
}

/***********************************************************************************************************************************
Convert JSON to KeyValue object

Currently this function is only intended to convert the limited types that are included in info files.  More types will be added as
needed.  Since this function is only intended to read internally-generated JSON it is assumed to be well-formed with no extraneous
whitespace.
***********************************************************************************************************************************/
KeyValue *
jsonToKv(const String *json)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(STRING, json);

        FUNCTION_TEST_ASSERT(json != NULL);
    FUNCTION_DEBUG_END();

    KeyValue *result = kvNew();

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // We'll examine the string byte by byte
        const char *jsonC = strPtr(json);
        unsigned int jsonPos = 0;

        // Consume the initial delimiter
        if (jsonC[jsonPos] == '[')
            THROW(JsonFormatError, "arrays not supported");
        else if (jsonC[jsonPos] != '{')
            THROW_FMT(JsonFormatError, "expected '{' but found '%c'", jsonC[jsonPos]);

        // Start parsing key/value pairs
        do
        {
            Variant *value = NULL;

            jsonPos++;

            // Parse the key which should always be quoted
            if (jsonC[jsonPos] != '"')
                THROW_FMT(JsonFormatError, "expected '\"' but found '%c'", jsonC[jsonPos]);

            unsigned int keyBeginPos = ++jsonPos;

            while (jsonC[jsonPos] != '"' && jsonPos < strSize(json) - 1)
                jsonPos++;

            if (jsonC[jsonPos] != '"')
                THROW_FMT(JsonFormatError, "expected '\"' but found '%c'", jsonC[jsonPos]);

            String *key = strNewN(jsonC + keyBeginPos, jsonPos - keyBeginPos);

            if (strSize(key) == 0)
                THROW(JsonFormatError, "zero-length key not allowed");

            if (jsonC[++jsonPos] != ':')
                THROW_FMT(JsonFormatError, "expected ':' but found '%c'", jsonC[jsonPos]);

            unsigned int valueBeginPos = ++jsonPos;

            // The value appears to be a string
            if (jsonC[jsonPos] == '"')
            {
                value = jsonString(jsonC, strSize(json), &jsonPos, &valueBeginPos);
            }

            // The value appears to be a number
            else if (isdigit(jsonC[jsonPos]))
            {
                value = jsonNumeric(jsonC, strSize(json), &jsonPos, &valueBeginPos);
            }

            // The value appears to be a boolean
            else if (jsonC[jsonPos] == 't' || jsonC[jsonPos] == 'f')
            {
                valueBeginPos = jsonPos;

                while (jsonC[jsonPos] != 'e' && jsonPos < strSize(json) - 1)
                    jsonPos++;

                if (jsonC[jsonPos] != 'e')
                    THROW_FMT(JsonFormatError, "expected boolean but found '%c'", jsonC[jsonPos]);

                jsonPos++;

                String *valueStr = strNewN(jsonC + valueBeginPos, jsonPos - valueBeginPos);

                if (strCmpZ(valueStr, "true") != 0 && strCmpZ(valueStr, "false") != 0)
                    THROW_FMT(JsonFormatError, "expected 'true' or 'false' but found '%s'", strPtr(valueStr));

                value = varNewBool(varBoolForce(varNewStr(valueStr)));
            }

            // The value appears to be an array.
            else if (jsonC[jsonPos] == '[')
            {
                // Add a pointer to an empty variant list as the value for the key.
                Variant *valueList = varNewVarLst(varLstNew());
                kvAdd(result, varNewStr(key), valueList);

                unsigned char arrayType = '\0';
                // ??? Currently only working with same-type simple single-dimensional arrays
                do
                {
                    jsonPos++;
                    valueBeginPos = jsonPos;

                    // The value appears to be a string
                    if (jsonC[jsonPos] == '"')
                    {
                        if (arrayType != '\0' && arrayType != 's')
                            THROW_FMT(JsonFormatError, "string found in array of type '%c'", arrayType);

                        arrayType = 's';

                        value = jsonString(jsonC, strSize(json), &jsonPos, &valueBeginPos);
                    }

                    // The value appears to be a number
                    else if (isdigit(jsonC[jsonPos]))
                    {
                        if (arrayType != '\0' && arrayType != 'n')
                            THROW_FMT(JsonFormatError, "number found in array of type '%c'", arrayType);

                        arrayType = 'n';

                        value = jsonNumeric(jsonC, strSize(json), &jsonPos, &valueBeginPos);
                    }

                    else
                        THROW(JsonFormatError, "unknown array value type");

                    kvAdd(result, varNewStr(key), value);
                }
                while (jsonC[jsonPos] == ',');

                if (jsonC[jsonPos] != ']')
                    THROW_FMT(JsonFormatError, "expected array delimeter ']' but found '%c'", jsonC[jsonPos]);

                jsonPos++;

                continue;
            }
            // Else not sure what it is.  Currently nulls will error.
// CSHANG: NULLS - NULLS and empty are different in JSON. If we got here, we could just check for jsonC[jsonPos] == 'n' and then make sure it
// is null: String *valueStr = strNewN(jsonC[jsonPos], 5); if strCmpZ(valueStr, "null,") != 0 || strCmpZ(valueStr, "null}") != 0 then
// error else advance the jsonPos to the , or } and set value = NULL;  DO WE HAVE ANY OF THESE IN OUR INFO/CONF/MAINFEST FILES? AS
// far as I can see we do not.
// CSHANG And how to we handle empty objects like pg_data={}?

            else
                THROW(JsonFormatError, "unknown value type");

            kvPut(result, varNewStr(key), value);
        }
        while (jsonC[jsonPos] == ',');

        // Look for end delimiter
        if (jsonC[jsonPos] != '}')
            THROW_FMT(JsonFormatError, "expected '}' but found '%c'", jsonC[jsonPos]);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_DEBUG_RESULT(KEY_VALUE, result);
}

/***********************************************************************************************************************************
Convert KeyValue object to JSON string

Currently this function is only intended to convert the limited types that are included in info files.  More types will be added as
needed.  Since this function is only intended to read internally-generated JSON it is assumed to be well-formed with no extraneous
whitespace.
***********************************************************************************************************************************/
String *
kvToJson(const KeyValue *kv)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(KEY_VALUE, kv);

        FUNCTION_TEST_ASSERT(kv != NULL);
    FUNCTION_DEBUG_END();

    String *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        const VariantList *keyList = kvKeyList(kv);

        for (unsigned int keyIdx = 0; keyIdx < varLstSize(keyList); keyIdx++)
        {
            const Variant *value = kvGet(kv, varLstGet(keyList, keyIdx));

            // If going to add another key, prepend a comma
            if (keyIdx > 0)
                strCat(result, ",");

            // Keys are always strings, so add starting quote
            if (result == NULL)
                result = strNewFmt("\"%s\"=", strPtr(varStr(varLstGet(keyList, keyIdx))));
            else
                strCatFmt(result, "\"%s\"=", strPtr(varStr(varLstGet(keyList, keyIdx))));

            if (varType(value) == varTypeKeyValue)
            {
                printf("KEY: %s, IDX: %u, VALUE IS KV\n", strPtr(varStr(varLstGet(keyList, keyIdx))), keyIdx); fflush(stdout);
                KeyValue *data = kvDup(varKv(value));
                strCat(result, strPtr(kvToJson(data)));
            }
            else if (varType(value) == varTypeVariantList)
            {
                printf("KEY: %s, IDX: %u, VALUE IS VARLIST\n", strPtr(varStr(varLstGet(keyList, keyIdx))), keyIdx); fflush(stdout);
                // Open the array
                strCat(result, "[");

                for (unsigned int arrayIdx = 0; arrayIdx < varLstSize(varVarLst(value)); arrayIdx++)
                {
                    // varStrForce will force a boolean to true or false which is consistent with json
                    String *arrayValue = varStrForce(varLstGet(varVarLst(value), arrayIdx));
                    unsigned int type = varType(varLstGet(varVarLst(value), arrayIdx));

                    // If going to add another element, prepend a comma
                    if (arrayIdx > 0)
                        strCat(result, ",");

                    // If the type is a string, add leading and trailing double quotes
                    if (type == varTypeString)
                        strCatFmt(result, "\"%s\"", strPtr(arrayValue));
                    else
                        strCat(result, strPtr(arrayValue));

                    printf("     TYPE: %u, IDX: %u, VALUE: %s\n", type, arrayIdx, strPtr(arrayValue)); fflush(stdout);
                }

                // Close the array
                strCat(result, "]");
                printf("RESULT: %s\n", strPtr(result)); fflush(stdout);
            }
            else if (varType(value) == varTypeString)
            {
                strCatFmt(result, "\"%s\"", strPtr(varStr(value)));
                printf("KEY: %s, IDX: %u, VALUE IS STRING\n", strPtr(varStr(varLstGet(keyList, keyIdx))), keyIdx); fflush(stdout);
            }
            else
                strCat(result, strPtr(varStrForce(value)));
        }

// CSHANG May have to move the memory context?

    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_DEBUG_RESULT(STRING, result);
}
