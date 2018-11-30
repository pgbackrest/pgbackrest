/***********************************************************************************************************************************
Convert JSON to/from KeyValue
***********************************************************************************************************************************/
#include <ctype.h>

#include "common/debug.h"
#include "common/log.h"
#include "common/type/json.h"

/***********************************************************************************************************************************
Given a character array and its size, return a variant from the extracted string
***********************************************************************************************************************************/
static Variant *
jsonString(const char *jsonC, size_t strSize, unsigned int *jsonPos, unsigned int *valueBeginPos)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(CHARPY, jsonC);
        FUNCTION_TEST_PARAM(SIZE, strSize);
        FUNCTION_TEST_PARAM(UINTP, jsonPos);
        FUNCTION_TEST_PARAM(UINTP, valueBeginPos);
    FUNCTION_TEST_END();

    Variant *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        *valueBeginPos = *valueBeginPos + 1;
        *jsonPos = *jsonPos + 1;

        // Find the end of the string within the entire character array
        while (jsonC[*jsonPos] != '"' && *jsonPos < strSize - 1)
            *jsonPos = *jsonPos + 1;

        if (jsonC[*jsonPos] != '"')
            THROW_FMT(JsonFormatError, "expected '\"' but found '%c'", jsonC[*jsonPos]);

        // Extract the string, including the enclosing quotes and return it as a variant
        String *resultStr = strNewN(jsonC + *valueBeginPos, *jsonPos - *valueBeginPos);
        memContextSwitch(MEM_CONTEXT_OLD());
        result = varNewStr(resultStr);
        memContextSwitch(MEM_CONTEXT_TEMP());

        // Advance the character array pointer to the next element after the string
        *jsonPos = *jsonPos + 1;
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_TEST_RESULT(VARIANT, result);
}

/***********************************************************************************************************************************
Given a character array and its size, return a variant from the extracted numeric
***********************************************************************************************************************************/
static Variant *
jsonNumeric(const char *jsonC, size_t strSize, unsigned int *jsonPos, unsigned int *valueBeginPos)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(CHARPY, jsonC);
        FUNCTION_TEST_PARAM(SIZE, strSize);
        FUNCTION_TEST_PARAM(UINTP, jsonPos);
        FUNCTION_TEST_PARAM(UINTP, valueBeginPos);
    FUNCTION_TEST_END();

    Variant *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Find the end of the numeric within the entire character array
        while (isdigit(jsonC[*jsonPos]) && *jsonPos < strSize - 1)
            *jsonPos = *jsonPos + 1;

        // Extract the numeric as a string
        String *resultStr = strNewN(jsonC + *valueBeginPos, *jsonPos - *valueBeginPos);

        // Convert the string to a uint64 variant
        memContextSwitch(MEM_CONTEXT_OLD());
        result = varNewUInt64(cvtZToUInt64(strPtr(resultStr)));
        memContextSwitch(MEM_CONTEXT_TEMP());
    }
    MEM_CONTEXT_TEMP_END();

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

                // ??? Currently only working with same-type simple single-dimensional arrays
                unsigned char arrayType = '\0';

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
Internal recursive function to walk a KeyValue and return a json string
***********************************************************************************************************************************/
static String *
kvToJsonInternal(const KeyValue *kv, String *indentSpace, String *indentDepth)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(KEY_VALUE, kv);
        FUNCTION_TEST_PARAM(STRING, indentSpace);
        FUNCTION_TEST_PARAM(STRING, indentDepth);

        FUNCTION_TEST_ASSERT(kv != NULL);
        FUNCTION_TEST_ASSERT(indentSpace != NULL);
        FUNCTION_TEST_ASSERT(indentDepth != NULL);
    FUNCTION_TEST_END();

    String *result = strNew("{");

    MEM_CONTEXT_TEMP_BEGIN()
    {
        const StringList *keyList = strLstSort(strLstNewVarLst(kvKeyList(kv)), sortOrderAsc);

        // If not an empty list, then add indent formatting
        if (strLstSize(keyList) > 0)
            strCatFmt(result, "%s", strPtr(indentDepth));

        for (unsigned int keyIdx = 0; keyIdx < strLstSize(keyList); keyIdx++)
        {
            String *key = strLstGet(keyList, keyIdx);
            const Variant *value = kvGet(kv, varNewStr(key));

            // If going to add another key, prepend a comma
            if (keyIdx > 0)
                strCatFmt(result, ",%s", strPtr(indentDepth));

            // Keys are always strings in the output, so add starting quote and colon.
            // Note if indent is 0 then spaces do not surround the colon, else they do.
            if (strSize(indentSpace) == 0)
                strCatFmt(result, "\"%s\":", strPtr(key));
            else
                strCatFmt(result, "\"%s\" : ", strPtr(key));

            // NULL value
            if (value == NULL)
                strCat(result, "null");
            // KeyValue
            else if (varType(value) == varTypeKeyValue)
            {
                strCat(indentDepth, strPtr(indentSpace));
                strCat(result, strPtr(kvToJsonInternal(kvDup(varKv(value)), indentSpace, indentDepth)));
            }
            // VariantList
            else if (varType(value) == varTypeVariantList)
            {
                // If the array is empty, then do not add formatting, else process the array.
                if (varLstSize(varVarLst(value)) == 0)
                    strCat(result, "[]");
                else
                {
                    strCat(indentDepth, strPtr(indentSpace));
                    strCatFmt(result, "[%s", strPtr(indentDepth));

                    for (unsigned int arrayIdx = 0; arrayIdx < varLstSize(varVarLst(value)); arrayIdx++)
                    {
                        Variant *arrayValue = varLstGet(varVarLst(value), arrayIdx);
                        unsigned int type = varType(varLstGet(varVarLst(value), arrayIdx));

                        // If going to add another element, add a comma
                        if (arrayIdx > 0)
                            strCatFmt(result, ",%s", strPtr(indentDepth));

                        // If the type is a string, add leading and trailing double quotes
                        if (type == varTypeString)
                            strCatFmt(result, "\"%s\"", strPtr(varStr(arrayValue)));
                        else if (type == varTypeKeyValue)
                        {
                            strCat(indentDepth, strPtr(indentSpace));
                            strCat(result, strPtr(kvToJsonInternal(kvDup(varKv(arrayValue)), indentSpace, indentDepth)));
                        }
                        // Numeric, Boolean or other type
                        else
                            strCat(result, strPtr(varStrForce(arrayValue)));
                    }

                    if (strSize(indentDepth) > strSize(indentSpace))
                        strTrunc(indentDepth, (int)(strSize(indentDepth) - strSize(indentSpace)));

                    strCatFmt(result, "%s]", strPtr(indentDepth));
                }
            }
            // String
            else if (varType(value) == varTypeString)
                strCatFmt(result, "\"%s\"", strPtr(varStr(value)));
            // Numeric, Boolean or other type
            else
                strCat(result, strPtr(varStrForce(value)));
        }
        if (strSize(indentDepth) > strSize(indentSpace))
            strTrunc(indentDepth, (int)(strSize(indentDepth) - strSize(indentSpace)));

        if (strLstSize(keyList) > 0)
            strCatFmt(result, "%s}", strPtr(indentDepth));
        else
            result = strCat(result, "}");

    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_TEST_RESULT(STRING, result);
}

/***********************************************************************************************************************************
Convert KeyValue object to JSON string. If indent = 0 then no pretty format.

Currently this function is only intended to convert the limited types that are included in info files.  More types will be added as
needed.  Since this function is only intended to read internally-generated JSON it is assumed to be well-formed with no extraneous
whitespace.
***********************************************************************************************************************************/
String *
kvToJson(const KeyValue *kv, unsigned int indent)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(KEY_VALUE, kv);
        FUNCTION_DEBUG_PARAM(UINT, indent);

        FUNCTION_DEBUG_ASSERT(kv != NULL);
    FUNCTION_DEBUG_END();

    String *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        String *jsonStr = strNew("");
        String *indentSpace = strNew("");
        String *indentDepth = strNew("");

        // Set up the indent spacing (indent 0 will result in an empty string)
        for (unsigned int indentIdx = 0; indentIdx < indent; indentIdx++)
            strCat(indentSpace, " ");

        // If indent > 0 (pretty printing) then add carriage return to the indent format
        if (indent > 0)
            strCat(indentDepth, "\n");

        strCat(indentDepth, strPtr(indentSpace));
        strCat(jsonStr, strPtr(kvToJsonInternal(kv, indentSpace, indentDepth)));

        // Add terminating linefeed for pretty print if it is not already added
        if (indent > 0 && !strEndsWithZ(jsonStr, "\n"))
            strCat(jsonStr, "\n");

        // Duplicate the string into the calling context
        memContextSwitch(MEM_CONTEXT_OLD());
        result = strDup(jsonStr);
        memContextSwitch(MEM_CONTEXT_TEMP());
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_DEBUG_RESULT(STRING, result);
}

/***********************************************************************************************************************************
Convert Variant object to JSON string. If indent = 0 then no pretty format.

Currently this function is only intended to convert the limited types that are included in info files.  More types will be added as
needed.
***********************************************************************************************************************************/
String *
varToJson(const Variant *var, unsigned int indent)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(VARIANT, var);
        FUNCTION_DEBUG_PARAM(UINT, indent);

        FUNCTION_DEBUG_ASSERT(var != NULL);
    FUNCTION_DEBUG_END();

    String *result = NULL;

    // Currently the variant to parse must be either a VariantList or a KeyValue type.
    if (varType(var) != varTypeVariantList && varType(var) != varTypeKeyValue)
        THROW(JsonFormatError, "variant type is invalid");

    MEM_CONTEXT_TEMP_BEGIN()
    {
        String *jsonStr = strNew("");
        String *indentSpace = strNew("");
        String *indentDepth = strNew("");

        // Set up the indent spacing (indent 0 will result in an empty string)
        for (unsigned int indentIdx = 0; indentIdx < indent; indentIdx++)
            strCat(indentSpace, " ");

        // If indent > 0 (pretty printing) then add carriage return to the indent format
        if (indent > 0)
            strCat(indentDepth, "\n");

        strCat(indentDepth, strPtr(indentSpace));

        // If VariantList then process each item in the array. Currently the list must be KeyValue types.
        if (varType(var) == varTypeVariantList)
        {
            const VariantList *vl = varVarLst(var);

            // If not an empty array
            if (varLstSize(vl) > 0)
            {
                // Add the indent formatting
                strCatFmt(jsonStr, "[%s", strPtr(indentDepth));

                // Currently only KeyValue list is supported
                for (unsigned int vlIdx = 0; vlIdx < varLstSize(vl); vlIdx++)
                {
                    // If going to add another key, append a comma and format for the next line
                    if (vlIdx > 0)
                        strCatFmt(jsonStr, ",%s", strPtr(indentDepth));

                    // Update the depth before processing the contents of the list element
                    strCat(indentDepth, strPtr(indentSpace));
                    strCat(jsonStr, strPtr(kvToJsonInternal(varKv(varLstGet(vl, vlIdx)), indentSpace, indentDepth)));
                }

                // Decrease the depth
                if (strSize(indentDepth) > strSize(indentSpace))
                    strTrunc(indentDepth, (int)(strSize(indentDepth) - strSize(indentSpace)));

                // Close the array
                strCatFmt(jsonStr, "%s]", strPtr(indentDepth));
            }
            // Else empty array
            else
                strCat(jsonStr, "[]");
        }
        // Else just convert the KeyValue
        else
            strCat(jsonStr, strPtr(kvToJsonInternal(varKv(var), indentSpace, indentDepth)));

        // Add terminating linefeed for pretty print if it is not already added
        if (indent > 0 && !strEndsWithZ(jsonStr, "\n"))
            strCat(jsonStr, "\n");

        // Duplicate the string into the calling context
        memContextSwitch(MEM_CONTEXT_OLD());
        result = strDup(jsonStr);
        memContextSwitch(MEM_CONTEXT_TEMP());
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_DEBUG_RESULT(STRING, result);
}
