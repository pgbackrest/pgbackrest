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
// CSHANG DO I need memcontext here?
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
// CSHANG DO I need memcontext here?
    while (isdigit(jsonC[*jsonPos]) && *jsonPos < strSize - 1)
        *jsonPos = *jsonPos + 1;

    FUNCTION_TEST_RESULT(VARIANT, varNewUInt64(cvtZToUInt64(strPtr(strNewN(jsonC + *valueBeginPos, *jsonPos - *valueBeginPos)))));
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
// far as I can see we do not. BUT we do have them for the info command so that would need to be in kvToJson
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

    String *result = strNew("{");

    MEM_CONTEXT_TEMP_BEGIN()
    {
        const StringList *keyList = strLstSort(strLstNewVarLst(kvKeyList(kv)), sortOrderAsc);

        for (unsigned int keyIdx = 0; keyIdx < strLstSize(keyList); keyIdx++)
        {
            String *key = strLstGet(keyList, keyIdx);
            const Variant *value = kvGet(kv, varNewStr(key));

            // If going to add another key, prepend a comma
            if (keyIdx > 0)
                strCat(result, ",");

            // Keys are always strings in the output, so add starting quote
            strCatFmt(result, "\"%s\":", strPtr(key));

            if (varType(value) == varTypeKeyValue)
            {
                KeyValue *data = kvDup(varKv(value));
                strCat(result, strPtr(kvToJson(data)));
            }
            else if (varType(value) == varTypeVariantList)
            {
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
                }

                // Close the array
                strCat(result, "]");
            }
            else if (varType(value) == varTypeString)
                strCatFmt(result, "\"%s\"", strPtr(varStr(value)));
// CSHANG More is needed to process NULL
            else
                strCat(result, strPtr(varStrForce(value)));
        }
        strCat(result, "}");
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_DEBUG_RESULT(STRING, result);
}

/***********************************************************************************************************************************
Pretty print a json string. Indent must be a value 0 - 8.
***********************************************************************************************************************************/
String *
jsonPretty(const String *json, unsigned int indent)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(STRING, json);
        FUNCTION_DEBUG_PARAM(UINT, indent);

        FUNCTION_TEST_ASSERT(json != NULL);
        FUNCTION_TEST_ASSERT(indent <= 8);
    FUNCTION_DEBUG_END();

    String *result = strNew("");

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // We'll examine the string byte by byte
        const char *jsonC = strPtr(json);
        unsigned int jsonPos = 0;

        // Confirm the initial delimiter
        if (jsonC[jsonPos] != '[' && jsonC[jsonPos] != '{')
            THROW_FMT(JsonFormatError, "expected '{' but found '%c'", jsonC[jsonPos]);

        String *indentSpace = strNew("");

        for (unsigned int idx = 0; idx < indent; idx++)
        {
            strCat(indentSpace, " ");
        }
// CSHANG Need to figure out how to ensure the json string passed has the first position with outer bracket for array
        // if (jsonC[jsonPos] != '[')
        //     strCatFmt(result, "[\n%s", strPtr(indentSpace));

// CSHANG Maybe all this would be better done with buffers?
        unsigned int depth = 1;


// CSHANG Below should be a recursive function into which depth, the json string, json position, indentSpace and indentDepth are passed
// and return result? PROBLEM - Can have {} or [] or null so can't always say { and [ have a carriace return after
        if (jsonC[jsonPos] == '{')
        {
            if (jsonC[jsonPos + 1] != '}')
            {
                strCat(indentDepth, strPtr(indentSpace));
                strCatFmt(result, "%s\n%s", jsonC[jsonPos], strPtr(indentDepth));
            }
        }
        else if (jsonC[jsonPos] == '[')
        {
            // If not an empty array, increase indentation and add a carriage return followed by the new indentation
            if (jsonC[jsonPos + 1] != ']')
            {
                strCat(indentDepth, strPtr(indentSpace));
                strCatFmt(result, "%s\n%s", jsonC[jsonPos], strPtr(indentDepth));
            }
            else
            {
                // If it is an empty array, a comma should follow unless it is the end of the string
                if (jsonC[jsonPos + 2] != ',' && jsonPos + 1 < strSize(json) - 1)
                    THROW_FMT(JsonFormatError, "expected ',' but found '%c'", jsonC[jsonPos +2]);

                strCatFmt(result, "%s%s,", jsonC[jsonPos], jsonC[jsonPos+1]);
                jsonPos = jsonPos + 2;
            }
        }
        else if (jsonC[jsonPos] == ',')
        {
            strCatFmt(result, "%s\n%s", jsonC[jsonPos], strPtr(indentDepth));
        }
        else if (jsonC[jsonPos] == '}' || jsonC[jsonPos] == ']')
        {
            strTrunc(indentDepth, strSize(indentDepth) - strSize(indentSpace));
            strCatFmt(result, "\n%s%s", strPtr(indentDepth), jsonC[jsonPos]);
        }
        else if (jsonC[jsonPos] == ':')
        {
            strCatFmt(result, " %s ", jsonC[jsonPos]);
        }
        else if (jsonC[jsonPos] == '"')
        {
            // Set valueBeginPos to jsonPos before incrementing jsonPos
            unsigned int valueBeginPos = jsonPos++;

            while (jsonC[jsonPos] != '"' && jsonPos < strSize(json) - 1)
                jsonPos = jsonPos + 1;

            if (jsonC[jsonPos] != '"')
                THROW_FMT(JsonFormatError, "expected '\"' but found '%c'", jsonC[*jsonPos]);

            // Copy the string, including beginning and ending quotes
            jsonPos = jsonPos + 1;
            strCat(result, strPtr(strNewN(jsonC + valueBeginPos, jsonPos - valueBeginPos)));
        }

    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_DEBUG_RESULT(STRING, result);
}
