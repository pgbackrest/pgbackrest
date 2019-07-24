/***********************************************************************************************************************************
Convert JSON to/from KeyValue
***********************************************************************************************************************************/
#include "build.auto.h"

#include <ctype.h>
#include <string.h>

#include "common/debug.h"
#include "common/log.h"
#include "common/type/json.h"

/***********************************************************************************************************************************
Prototypes
***********************************************************************************************************************************/
static Variant *jsonToVarInternal(const char *json, unsigned int *jsonPos);

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
Convert a json string to a bool
***********************************************************************************************************************************/
static bool
jsonToBoolInternal(const char *json, unsigned int *jsonPos)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRINGZ, json);
        FUNCTION_TEST_PARAM_P(UINT, jsonPos);
    FUNCTION_TEST_END();

    bool result;

    if (strncmp(json + *jsonPos, "true", 4) == 0)
    {
        result = true;
        *jsonPos += 4;
    }
    else if (strncmp(json + *jsonPos, "false", 5) == 0)
    {
        result = false;
        *jsonPos += 5;
    }
    else
        THROW_FMT(JsonFormatError, "expected boolean at '%s'", json + *jsonPos);

    FUNCTION_TEST_RETURN(result);
}

bool
jsonToBool(const String *json)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, json);
    FUNCTION_TEST_END();

    unsigned int jsonPos = 0;
    jsonConsumeWhiteSpace(strPtr(json), &jsonPos);

    bool result = jsonToBoolInternal(strPtr(json), &jsonPos);

    jsonConsumeWhiteSpace(strPtr(json), &jsonPos);

    if (jsonPos != strSize(json))
        THROW_FMT(JsonFormatError, "unexpected characters after boolean at '%s'", strPtr(json) + jsonPos);

    FUNCTION_TEST_RETURN(result);
}

/***********************************************************************************************************************************
Convert a json number to various integer types
***********************************************************************************************************************************/
static Variant *
jsonToNumberInternal(const char *json, unsigned int *jsonPos)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRINGZ, json);
        FUNCTION_TEST_PARAM_P(UINT, jsonPos);
    FUNCTION_TEST_END();

    Variant *result = NULL;
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

        // Convert the string to a integer variant
        memContextSwitch(MEM_CONTEXT_OLD());

        if (intSigned)
            result = varNewInt64(cvtZToInt64(strPtr(resultStr)));
        else
            result = varNewUInt64(cvtZToUInt64(strPtr(resultStr)));

        memContextSwitch(MEM_CONTEXT_TEMP());
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_TEST_RETURN(result);
}

static Variant *
jsonToNumber(const String *json)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, json);
    FUNCTION_TEST_END();

    unsigned int jsonPos = 0;
    jsonConsumeWhiteSpace(strPtr(json), &jsonPos);

    Variant *result = jsonToNumberInternal(strPtr(json), &jsonPos);

    jsonConsumeWhiteSpace(strPtr(json), &jsonPos);

    if (jsonPos != strSize(json))
        THROW_FMT(JsonFormatError, "unexpected characters after number at '%s'", strPtr(json) + jsonPos);

    FUNCTION_TEST_RETURN(result);
}

int
jsonToInt(const String *json)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, json);
    FUNCTION_TEST_END();

    int result = 0;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        result = varIntForce(jsonToNumber(json));
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_TEST_RETURN(result);
}

int64_t
jsonToInt64(const String *json)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, json);
    FUNCTION_TEST_END();

    int64_t result = 0;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        result = varInt64Force(jsonToNumber(json));
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_TEST_RETURN(result);
}

unsigned int
jsonToUInt(const String *json)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, json);
    FUNCTION_TEST_END();

    unsigned int result = 0;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        result = varUIntForce(jsonToNumber(json));
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_TEST_RETURN(result);
}

uint64_t
jsonToUInt64(const String *json)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, json);
    FUNCTION_TEST_END();

    uint64_t result = 0;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        result = varUInt64Force(jsonToNumber(json));
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_TEST_RETURN(result);
}

/***********************************************************************************************************************************
Convert a json string to a String
***********************************************************************************************************************************/
static String *
jsonToStrInternal(const char *json, unsigned int *jsonPos)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRINGZ, json);
        FUNCTION_TEST_PARAM_P(UINT, jsonPos);
    FUNCTION_TEST_END();

    String *result = strNew("");

    if (json[*jsonPos] != '"')
        THROW_FMT(JsonFormatError, "expected '\"' at '%s'", json + *jsonPos);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        (*jsonPos)++;

        while (json[*jsonPos] != '"')
        {
            if (json[*jsonPos] == '\\')
            {
                (*jsonPos)++;;

                switch (json[*jsonPos])
                {
                    case '"':
                            strCatChr(result, '"');
                        break;

                    case '\\':
                            strCatChr(result, '\\');
                        break;

                    case '/':
                            strCatChr(result, '/');
                        break;

                    case 'n':
                            strCatChr(result, '\n');
                        break;

                    case 'r':
                            strCatChr(result, '\r');
                        break;

                    case 't':
                            strCatChr(result, '\t');
                        break;

                    case 'b':
                            strCatChr(result, '\b');
                        break;

                    case 'f':
                            strCatChr(result, '\f');
                        break;

                    default:
                        THROW_FMT(JsonFormatError, "invalid escape character '%c'", json[*jsonPos]);
                }
            }
            else
            {
                if (json[*jsonPos] == '\0')
                    THROW(JsonFormatError, "expected '\"' but found null delimiter");

                    strCatChr(result, json[*jsonPos]);
            }

            (*jsonPos)++;;
        };

        // Advance the character array pointer to the next element after the string
        (*jsonPos)++;;
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_TEST_RETURN(result);
}

String *
jsonToStr(const String *json)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, json);
    FUNCTION_TEST_END();

    unsigned int jsonPos = 0;
    jsonConsumeWhiteSpace(strPtr(json), &jsonPos);

    String *result = jsonToStrInternal(strPtr(json), &jsonPos);

    jsonConsumeWhiteSpace(strPtr(json), &jsonPos);

    if (jsonPos != strSize(json))
        THROW_FMT(JsonFormatError, "unexpected characters after string at '%s'", strPtr(json) + jsonPos);

    FUNCTION_TEST_RETURN(result);
}

/***********************************************************************************************************************************
Convert a json object to a KeyValue
***********************************************************************************************************************************/
static KeyValue *
jsonToKvInternal(const char *json, unsigned int *jsonPos)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRINGZ, json);
        FUNCTION_TEST_PARAM_P(UINT, jsonPos);
    FUNCTION_TEST_END();

    KeyValue *result = kvNew();

    MEM_CONTEXT_TEMP_BEGIN()
    {
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

                Variant *key = varNewStr(jsonToStrInternal(json, jsonPos));

                jsonConsumeWhiteSpace(json, jsonPos);

                if (json[*jsonPos] != ':')
                    THROW_FMT(JsonFormatError, "expected ':' at '%s'", json + *jsonPos);
                (*jsonPos)++;

                jsonConsumeWhiteSpace(json, jsonPos);

                kvPut(result, key, jsonToVarInternal(json, jsonPos));
            }
            while (json[*jsonPos] == ',');
        }

        if (json[*jsonPos] != '}')
            THROW_FMT(JsonFormatError, "expected '}' at '%s'", json + *jsonPos);
        (*jsonPos)++;
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_TEST_RETURN(result);
}

KeyValue *
jsonToKv(const String *json)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, json);
    FUNCTION_TEST_END();

    unsigned int jsonPos = 0;
    jsonConsumeWhiteSpace(strPtr(json), &jsonPos);

    if (strPtr(json)[jsonPos] != '{')
        THROW_FMT(JsonFormatError, "expected '{' at '%s'", strPtr(json) + jsonPos);

    KeyValue *result = jsonToKvInternal(strPtr(json), &jsonPos);

    jsonConsumeWhiteSpace(strPtr(json), &jsonPos);

    if (jsonPos != strSize(json))
        THROW_FMT(JsonFormatError, "unexpected characters after object at '%s'", strPtr(json) + jsonPos);

    FUNCTION_TEST_RETURN(result);
}

/***********************************************************************************************************************************
Convert a json string to an array
***********************************************************************************************************************************/
static VariantList *
jsonToVarLstInternal(const char *json, unsigned int *jsonPos)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRINGZ, json);
        FUNCTION_TEST_PARAM_P(UINT, jsonPos);
    FUNCTION_TEST_END();

    VariantList *result = varLstNew();

    if (json[*jsonPos] != '[')
        THROW_FMT(JsonFormatError, "expected '[' at '%s'", json + *jsonPos);

    MEM_CONTEXT_TEMP_BEGIN()
    {
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

                memContextSwitch(MEM_CONTEXT_OLD());
                varLstAdd(result, jsonToVarInternal(json, jsonPos));
                memContextSwitch(MEM_CONTEXT_TEMP());

                jsonConsumeWhiteSpace(json, jsonPos);
            }
            while (json[*jsonPos] == ',');
        }

        if (json[*jsonPos] != ']')
            THROW_FMT(JsonFormatError, "expected ']' at '%s'", json + *jsonPos);

        (*jsonPos)++;
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_TEST_RETURN(result);
}

VariantList *
jsonToVarLst(const String *json)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, json);
    FUNCTION_TEST_END();

    unsigned int jsonPos = 0;
    jsonConsumeWhiteSpace(strPtr(json), &jsonPos);

    VariantList *result = jsonToVarLstInternal(strPtr(json), &jsonPos);

    jsonConsumeWhiteSpace(strPtr(json), &jsonPos);

    if (jsonPos != strSize(json))
        THROW_FMT(JsonFormatError, "unexpected characters after array at '%s'", strPtr(json) + jsonPos);

    FUNCTION_TEST_RETURN(result);
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
            result = varNewStr(jsonToStrInternal(json, jsonPos));
            break;
        }

        // Integer
        case '-':
        case '0' ... '9':
        {
            result = jsonToNumberInternal(json, jsonPos);
            break;
        }

        // Boolean
        case 't':
        case 'f':
        {
            result = varNewBool(jsonToBoolInternal(json, jsonPos));
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
            result = varNewVarLst(jsonToVarLstInternal(json, jsonPos));
            break;
        }

        // Object
        case '{':
        {
            result = varNewKv(jsonToKvInternal(json, jsonPos));
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
Convert a boolean to JSON
***********************************************************************************************************************************/
const String *
jsonFromBool(bool value)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(BOOL, value);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RETURN(value ? TRUE_STR : FALSE_STR);
}

/***********************************************************************************************************************************
Convert a number to JSON
***********************************************************************************************************************************/
String *
jsonFromInt(int number)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(INT, number);
    FUNCTION_TEST_END();

    char working[CVT_BASE10_BUFFER_SIZE];
    cvtIntToZ(number, working, sizeof(working));

    FUNCTION_TEST_RETURN(strNew(working));
}

String *
jsonFromInt64(int64_t number)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(INT64, number);
    FUNCTION_TEST_END();

    char working[CVT_BASE10_BUFFER_SIZE];
    cvtInt64ToZ(number, working, sizeof(working));

    FUNCTION_TEST_RETURN(strNew(working));
}

String *
jsonFromUInt(unsigned int number)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(UINT, number);
    FUNCTION_TEST_END();

    char working[CVT_BASE10_BUFFER_SIZE];
    cvtUIntToZ(number, working, sizeof(working));

    FUNCTION_TEST_RETURN(strNew(working));
}

String *
jsonFromUInt64(uint64_t number)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(UINT64, number);
    FUNCTION_TEST_END();

    char working[CVT_BASE10_BUFFER_SIZE];
    cvtUInt64ToZ(number, working, sizeof(working));

    FUNCTION_TEST_RETURN(strNew(working));
}

/***********************************************************************************************************************************
Output and escape a string
***********************************************************************************************************************************/
static void
jsonFromStrInternal(String *json, const String *string)
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

String *
jsonFromStr(const String *string)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, string);
    FUNCTION_TEST_END();

    String *json = strNew("");
    jsonFromStrInternal(json, string);

    FUNCTION_TEST_RETURN(json);
}

/***********************************************************************************************************************************
Internal recursive function to walk a KeyValue and return a json string
***********************************************************************************************************************************/
static String *
jsonFromKvInternal(const KeyValue *kv, String *indentSpace, String *indentDepth)
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
            const Variant *value = kvGet(kv, VARSTR(key));

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
                strCat(result, strPtr(jsonFromKvInternal(kvDup(varKv(value)), indentSpace, indentDepth)));
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
                            jsonFromStrInternal(result, varStr(arrayValue));
                        }
                        else if (varType(arrayValue) == varTypeKeyValue)
                        {
                            strCat(indentDepth, strPtr(indentSpace));
                            strCat(result, strPtr(jsonFromKvInternal(kvDup(varKv(arrayValue)), indentSpace, indentDepth)));
                        }
                        else if (varType(arrayValue) == varTypeVariantList)
                        {
                            strCat(indentDepth, strPtr(indentSpace));
                            strCat(result, strPtr(jsonFromVar(arrayValue, 0)));
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
                jsonFromStrInternal(result, varStr(value));
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
jsonFromKv(const KeyValue *kv, unsigned int indent)
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
        strCat(jsonStr, strPtr(jsonFromKvInternal(kv, indentSpace, indentDepth)));

        // Add terminating linefeed for pretty print
        if (indent > 0)
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
jsonFromVar(const Variant *var, unsigned int indent)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(VARIANT, var);
        FUNCTION_LOG_PARAM(UINT, indent);
    FUNCTION_LOG_END();

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

        // If VariantList then process each item in the array. Currently the list must be KeyValue types.
        if (var == NULL)
        {
            strCat(jsonStr, strPtr(NULL_STR));
        }
        else if (varType(var) == varTypeUInt)
        {
            strCat(jsonStr, strPtr(jsonFromUInt(varUInt(var))));
        }
        else if (varType(var) == varTypeUInt64)
        {
            strCat(jsonStr, strPtr(jsonFromUInt64(varUInt64(var))));
        }
        else if (varType(var) == varTypeString)
        {
            jsonFromStrInternal(jsonStr, varStr(var));
        }
        else if (varType(var) == varTypeVariantList)
        {
            const VariantList *vl = varVarLst(var);

            // If not an empty array
            if (varLstSize(vl) > 0)
            {
                // Add the indent formatting
                strCatFmt(jsonStr, "[%s", strPtr(indentDepth));

                // Currently only KeyValue and String lists are supported
                for (unsigned int vlIdx = 0; vlIdx < varLstSize(vl); vlIdx++)
                {
                    // If going to add another key, append a comma and format for the next line
                    if (vlIdx > 0)
                        strCatFmt(jsonStr, ",%s", strPtr(indentDepth));

                    Variant *varSub = varLstGet(vl, vlIdx);

                    if (varSub == NULL)
                    {
                        strCat(jsonStr, "null");
                    }
                    else if (varType(varSub) == varTypeBool)
                    {
                        strCat(jsonStr, strPtr(jsonFromBool(varBool(varSub))));
                    }
                    else if (varType(varSub) == varTypeKeyValue)
                    {
                        // Update the depth before processing the contents of the list element
                        strCat(indentDepth, strPtr(indentSpace));
                        strCat(jsonStr, strPtr(jsonFromKvInternal(varKv(varSub), indentSpace, indentDepth)));
                    }
                    else if (varType(varSub) == varTypeVariantList)
                    {
                        strCat(jsonStr, strPtr(jsonFromVar(varSub, indent)));
                    }
                    else if (varType(varSub) == varTypeInt64)
                    {
                        strCat(jsonStr, strPtr(jsonFromInt64(varInt64(varSub))));
                    }
                    else if (varType(varSub) == varTypeUInt)
                    {
                        strCat(jsonStr, strPtr(jsonFromUInt(varUInt(varSub))));
                    }
                    else if (varType(varSub) == varTypeUInt64)
                    {
                        strCat(jsonStr, strPtr(jsonFromUInt64(varUInt64(varSub))));
                    }
                    else
                        jsonFromStrInternal(jsonStr, varStr(varSub));
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
        else if (varType(var) == varTypeKeyValue)
        {
            strCat(jsonStr, strPtr(jsonFromKvInternal(varKv(var), indentSpace, indentDepth)));
        }
        else
            THROW(JsonFormatError, "variant type is invalid");

        // Add terminating linefeed for pretty print
        if (indent > 0)
            strCat(jsonStr, "\n");

        // Duplicate the string into the calling context
        memContextSwitch(MEM_CONTEXT_OLD());
        result = strDup(jsonStr);
        memContextSwitch(MEM_CONTEXT_TEMP());
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(STRING, result);
}
