/***********************************************************************************************************************************
Convert JSON to/from KeyValue
***********************************************************************************************************************************/
#include <ctype.h>
#include <string.h>

#include "common/debug.h"
#include "common/log.h"
#include "common/type/json.h"

/***********************************************************************************************************************************
Given a character array and its size, return a variant from the extracted string
***********************************************************************************************************************************/
static Variant *
jsonToStr(const char *json, unsigned int *jsonPos)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRINGZ, json);
        FUNCTION_TEST_PARAM_P(UINT, jsonPos);
    FUNCTION_TEST_END();

    Variant *result = NULL;

    if (json[*jsonPos] != '"')
        THROW_FMT(JsonFormatError, "expected '\"' at '%s'", json + *jsonPos);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        *jsonPos = *jsonPos + 1;

        String *resultStr = strNew("");

        while (json[*jsonPos] != '"')
        {
            if (json[*jsonPos] == '\\')
            {
                *jsonPos = *jsonPos + 1;

                switch (json[*jsonPos])
                {
                    case '"':
                            strCatChr(resultStr, '"');
                        break;

                    case '\\':
                            strCatChr(resultStr, '\\');
                        break;

                    case '/':
                            strCatChr(resultStr, '/');
                        break;

                    case 'n':
                            strCatChr(resultStr, '\n');
                        break;

                    case 'r':
                            strCatChr(resultStr, '\r');
                        break;

                    case 't':
                            strCatChr(resultStr, '\t');
                        break;

                    case 'b':
                            strCatChr(resultStr, '\b');
                        break;

                    case 'f':
                            strCatChr(resultStr, '\f');
                        break;

                    default:
                        THROW_FMT(JsonFormatError, "invalid escape character '%c'", json[*jsonPos]);
                }
            }
            else
            {
                if (json[*jsonPos] == '\0')
                    THROW(JsonFormatError, "expected '\"' but found null delimiter");

                    strCatChr(resultStr, json[*jsonPos]);
            }

            *jsonPos = *jsonPos + 1;
        };

        // Create a variant result for the string
        memContextSwitch(MEM_CONTEXT_OLD());
        result = varNewStr(resultStr);
        memContextSwitch(MEM_CONTEXT_TEMP());

        // Advance the character array pointer to the next element after the string
        *jsonPos = *jsonPos + 1;
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_TEST_RETURN(result);
}

/***********************************************************************************************************************************
Consume whitespace
***********************************************************************************************************************************/
static void
jsonConsumeWhiteSpace(const char *json, unsigned int *jsonPos)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRINGZ, json);
        FUNCTION_TEST_PARAM_P(UINT, jsonPos);
    FUNCTION_TEST_END();

    // Consume whitespace
    while (json[*jsonPos] == ' ' || json[*jsonPos] == '\t' || json[*jsonPos] == '\n'  || json[*jsonPos] == '\r')
        (*jsonPos)++;

    FUNCTION_TEST_RETURN_VOID();
}

/***********************************************************************************************************************************
Convert JSON to a variant
***********************************************************************************************************************************/
static Variant *
jsonToVarInternal(const char *json, unsigned int *jsonPos)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRINGZ, json);
        FUNCTION_TEST_PARAM_P(UINT, jsonPos);
    FUNCTION_TEST_END();

    Variant *result = NULL;

    jsonConsumeWhiteSpace(json, jsonPos);

    // There should be some data
    if (json[*jsonPos] == '\0')
        THROW(JsonFormatError, "expected data");

    // Determine data type
    switch (json[*jsonPos])
    {
        // String
        case '"':
        {
            result = jsonToStr(json, jsonPos);
            break;
        }

        // Integer
        case '-':
        case '0' ... '9':
        {
            unsigned int beginPos = *jsonPos;
            bool intSigned = false;

            // Consume the -
            if (json[*jsonPos] == '-')
            {
                (*jsonPos)++;
                intSigned = true;
            }

            // Consume all digits
            while (isdigit(json[*jsonPos]))
                (*jsonPos)++;

            // Invalid if only a - was found
            if (json[*jsonPos - 1] == '-')
                THROW_FMT(JsonFormatError, "found '-' with no integer at '%s'", json + beginPos);

            MEM_CONTEXT_TEMP_BEGIN()
            {
                // Extract the numeric as a string
                String *resultStr = strNewN(json + beginPos, *jsonPos - beginPos);

                // Convert the string to a int64 variant
                memContextSwitch(MEM_CONTEXT_OLD());

                if (intSigned)
                    result = varNewInt64(cvtZToInt64(strPtr(resultStr)));
                else
                    result = varNewUInt64(cvtZToUInt64(strPtr(resultStr)));

                memContextSwitch(MEM_CONTEXT_TEMP());
            }
            MEM_CONTEXT_TEMP_END();

            break;
        }

        // Boolean
        case 't':
        case 'f':
        {
            if (strncmp(json + *jsonPos, "true", 4) == 0)
            {
                result = varNewBool(true);
                *jsonPos += 4;
            }
            else if (strncmp(json + *jsonPos, "false", 5) == 0)
            {
                result = varNewBool(false);
                *jsonPos += 5;
            }
            else
                THROW_FMT(JsonFormatError, "expected boolean at '%s'", json + *jsonPos);

            break;
        }

        // Null
        case 'n':
        {
            if (strncmp(json + *jsonPos, "null", 4) == 0)
                *jsonPos += 4;
            else
                THROW_FMT(JsonFormatError, "expected null at '%s'", json + *jsonPos);

            break;
        }

        // Array
        case '[':
        {
            MEM_CONTEXT_TEMP_BEGIN()
            {
                VariantList *valueList = varLstNew();

                // Move position to the first element in the array
                (*jsonPos)++;
                jsonConsumeWhiteSpace(json, jsonPos);

                // Only proceed if the array is not empty
                if (json[*jsonPos] != ']')
                {
                    do
                    {
                        if (json[*jsonPos] == ',')
                        {
                            (*jsonPos)++;
                            jsonConsumeWhiteSpace(json, jsonPos);
                        }

                        varLstAdd(valueList, jsonToVarInternal(json, jsonPos));

                        jsonConsumeWhiteSpace(json, jsonPos);
                    }
                    while (json[*jsonPos] == ',');
                }

                if (json[*jsonPos] != ']')
                    THROW_FMT(JsonFormatError, "expected ']' at '%s'", json + *jsonPos);
                (*jsonPos)++;

                memContextSwitch(MEM_CONTEXT_OLD());
                result = varNewVarLst(varLstMove(valueList, MEM_CONTEXT_OLD()));
                memContextSwitch(MEM_CONTEXT_TEMP());
            }
            MEM_CONTEXT_TEMP_END();

            break;
        }

        // Object
        case '{':
        {
            MEM_CONTEXT_TEMP_BEGIN()
            {
                memContextSwitch(MEM_CONTEXT_OLD());
                result = varNewKv();
                memContextSwitch(MEM_CONTEXT_TEMP());

                // Move position to the first key/value in the object
                (*jsonPos)++;
                jsonConsumeWhiteSpace(json, jsonPos);

                // Only proceed if the array is not empty
                if (json[*jsonPos] != '}')
                {
                    do
                    {
                        if (json[*jsonPos] == ',')
                        {
                            (*jsonPos)++;
                            jsonConsumeWhiteSpace(json, jsonPos);
                        }

                        Variant *key = jsonToStr(json, jsonPos);

                        jsonConsumeWhiteSpace(json, jsonPos);

                        if (json[*jsonPos] != ':')
                            THROW_FMT(JsonFormatError, "expected ':' at '%s'", json + *jsonPos);
                        (*jsonPos)++;

                        jsonConsumeWhiteSpace(json, jsonPos);

                        kvPut(varKv(result), key, jsonToVarInternal(json, jsonPos));
                    }
                    while (json[*jsonPos] == ',');
                }

                if (json[*jsonPos] != '}')
                    THROW_FMT(JsonFormatError, "expected '}' at '%s'", json + *jsonPos);
                (*jsonPos)++;
            }
            MEM_CONTEXT_TEMP_END();

            break;
        }

        // Object
        default:
        {
            THROW_FMT(JsonFormatError, "invalid type at '%s'", json + *jsonPos);
            break;
        }
    }

    jsonConsumeWhiteSpace(json, jsonPos);

    FUNCTION_TEST_RETURN(result);
}

Variant *
jsonToVar(const String *json)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STRING, json);
    FUNCTION_LOG_END();

    const char *jsonPtr = strPtr(json);
    unsigned int jsonPos = 0;

    FUNCTION_LOG_RETURN(VARIANT, jsonToVarInternal(jsonPtr, &jsonPos));
}

/***********************************************************************************************************************************
Output and escape a string
***********************************************************************************************************************************/
static void
jsonStringRender(String *json, const String *string)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, json);
        FUNCTION_TEST_PARAM(STRING, string);
    FUNCTION_TEST_END();

    ASSERT(json != NULL);

    // If string is null
    if (string == NULL)
    {
        strCat(json, "null");
    }
    // Else escape and output string
    else
    {
        strCatChr(json, '"');

        for (unsigned int stringIdx = 0; stringIdx < strSize(string); stringIdx++)
        {
            char stringChr = strPtr(string)[stringIdx];

            switch (stringChr)
            {
                case '"':
                    strCat(json, "\\\"");
                    break;

                case '\\':
                    strCat(json, "\\\\");
                    break;

                case '/':
                    strCat(json, "\\/");
                    break;

                case '\n':
                    strCat(json, "\\n");
                    break;

                case '\r':
                    strCat(json, "\\r");
                    break;

                case '\t':
                    strCat(json, "\\t");
                    break;

                case '\b':
                    strCat(json, "\\b");
                    break;

                case '\f':
                    strCat(json, "\\f");
                    break;

                default:
                    strCatChr(json, stringChr);
                    break;
            }
        }

        strCatChr(json, '"');
    }

    FUNCTION_TEST_RETURN_VOID();
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
    FUNCTION_TEST_END();

    ASSERT(kv != NULL);
    ASSERT(indentSpace != NULL);
    ASSERT(indentDepth != NULL);

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
                if (varVarLst(value) == NULL)
                    strCat(result, "null");
                else if (varLstSize(varVarLst(value)) == 0)
                    strCat(result, "[]");
                else
                {
                    strCat(indentDepth, strPtr(indentSpace));
                    strCatFmt(result, "[%s", strPtr(indentDepth));

                    for (unsigned int arrayIdx = 0; arrayIdx < varLstSize(varVarLst(value)); arrayIdx++)
                    {
                        Variant *arrayValue = varLstGet(varVarLst(value), arrayIdx);

                        // If going to add another element, add a comma
                        if (arrayIdx > 0)
                            strCatFmt(result, ",%s", strPtr(indentDepth));

                        // If array value is null
                        if (arrayValue == NULL)
                        {
                            strCat(result, "null");
                        }
                        // If the type is a string, add leading and trailing double quotes
                        else if (varType(arrayValue) == varTypeString)
                        {
                            jsonStringRender(result, varStr(arrayValue));
                        }
                        else if (varType(arrayValue) == varTypeKeyValue)
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
            {
                jsonStringRender(result, varStr(value));
            }
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

    FUNCTION_TEST_RETURN(result);
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
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(KEY_VALUE, kv);
        FUNCTION_LOG_PARAM(UINT, indent);
    FUNCTION_LOG_END();

    ASSERT(kv != NULL);

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

    FUNCTION_LOG_RETURN(STRING, result);
}

/***********************************************************************************************************************************
Convert Variant object to JSON string. If indent = 0 then no pretty format.

Currently this function is only intended to convert the limited types that are included in info files.  More types will be added as
needed.
***********************************************************************************************************************************/
String *
varToJson(const Variant *var, unsigned int indent)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(VARIANT, var);
        FUNCTION_LOG_PARAM(UINT, indent);
    FUNCTION_LOG_END();

    ASSERT(var != NULL);

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

    FUNCTION_LOG_RETURN(STRING, result);
}
