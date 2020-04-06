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

/**********************************************************************************************************************************/
static bool
jsonToBoolInternal(const char *json, unsigned int *jsonPos)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRINGZ, json);
        FUNCTION_TEST_PARAM_P(UINT, jsonPos);
    FUNCTION_TEST_END();

    bool result;

    if (strncmp(json + *jsonPos, TRUE_Z, 4) == 0)
    {
        result = true;
        *jsonPos += 4;
    }
    else if (strncmp(json + *jsonPos, FALSE_Z, 5) == 0)
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

/**********************************************************************************************************************************/
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
        MEM_CONTEXT_PRIOR_BEGIN()
        {
            if (intSigned)
                result = varNewInt64(cvtZToInt64(strPtr(resultStr)));
            else
                result = varNewUInt64(cvtZToUInt64(strPtr(resultStr)));
        }
        MEM_CONTEXT_PRIOR_END();
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

/**********************************************************************************************************************************/
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
        // Skip the beginning "
        (*jsonPos)++;

        // Track portion of string with no escapes
        const char *noEscape = NULL;
        size_t noEscapeSize = 0;

        while (json[*jsonPos] != '"')
        {
            if (json[*jsonPos] == '\\')
            {
                // Copy portion of string without escapes
                if (noEscapeSize > 0)
                {
                    strCatZN(result, noEscape, noEscapeSize);
                    noEscapeSize = 0;
                }

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

                // If escape string is zero size then start it
                if (noEscapeSize == 0)
                    noEscape = json + *jsonPos;

                noEscapeSize++;
            }

            (*jsonPos)++;;
        };

        // Copy portion of string without escapes
        if (noEscapeSize > 0)
            strCatZN(result, noEscape, noEscapeSize);

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

    String *result = NULL;

    if (strncmp(strPtr(json), NULL_Z, 4) == 0)
        jsonPos += 4;
    else
        result = jsonToStrInternal(strPtr(json), &jsonPos);

    jsonConsumeWhiteSpace(strPtr(json), &jsonPos);

    if (jsonPos != strSize(json))
        THROW_FMT(JsonFormatError, "unexpected characters after string at '%s'", strPtr(json) + jsonPos);

    FUNCTION_TEST_RETURN(result);
}

/**********************************************************************************************************************************/
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

/**********************************************************************************************************************************/
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

                MEM_CONTEXT_PRIOR_BEGIN()
                {
                    varLstAdd(result, jsonToVarInternal(json, jsonPos));
                }
                MEM_CONTEXT_PRIOR_END();

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

/**********************************************************************************************************************************/
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
            if (strncmp(json + *jsonPos, NULL_Z, 4) == 0)
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

/**********************************************************************************************************************************/
const String *
jsonFromBool(bool value)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(BOOL, value);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RETURN(value ? TRUE_STR : FALSE_STR);
}

/**********************************************************************************************************************************/
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

/**********************************************************************************************************************************/
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
        strCat(json, NULL_Z);
    }
    // Else escape and output string
    else
    {
        strCatChr(json, '"');

        // Track portion of string with no escapes
        const char *noEscape = NULL;
        size_t noEscapeSize = 0;

        for (unsigned int stringIdx = 0; stringIdx < strSize(string); stringIdx++)
        {
            char stringChr = strPtr(string)[stringIdx];

            switch (stringChr)
            {
                case '"':
                case '\\':
                case '\n':
                case '\r':
                case '\t':
                case '\b':
                case '\f':
                {
                    // Copy portion of string without escapes
                    if (noEscapeSize > 0)
                    {
                        strCatZN(json, noEscape, noEscapeSize);
                        noEscapeSize = 0;
                    }

                    switch (stringChr)
                    {
                        case '"':
                        {
                            strCat(json, "\\\"");
                            break;
                        }

                        case '\\':
                        {
                            strCat(json, "\\\\");
                            break;
                        }

                        case '\n':
                        {
                            strCat(json, "\\n");
                            break;
                        }

                        case '\r':
                        {
                            strCat(json, "\\r");
                            break;
                        }

                        case '\t':
                        {
                            strCat(json, "\\t");
                            break;
                        }

                        case '\b':
                        {
                            strCat(json, "\\b");
                            break;
                        }

                        case '\f':
                        {
                            strCat(json, "\\f");
                            break;
                        }
                    }

                    break;
                }

                default:
                {
                    // If escape string is zero size then start it
                    if (noEscapeSize == 0)
                        noEscape = strPtr(string) + stringIdx;

                    noEscapeSize++;
                    break;
                }
            }
        }

        // Copy portion of string without escapes
        if (noEscapeSize > 0)
            strCatZN(json, noEscape, noEscapeSize);

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
jsonFromKvInternal(const KeyValue *kv)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(KEY_VALUE, kv);
    FUNCTION_TEST_END();

    ASSERT(kv != NULL);

    String *result = strNew("{");

    MEM_CONTEXT_TEMP_BEGIN()
    {
        const StringList *keyList = strLstSort(strLstNewVarLst(kvKeyList(kv)), sortOrderAsc);

        for (unsigned int keyIdx = 0; keyIdx < strLstSize(keyList); keyIdx++)
        {
            String *key = strLstGet(keyList, keyIdx);
            const Variant *value = kvGet(kv, VARSTR(key));

            // If going to add another key, prepend a comma
            if (keyIdx > 0)
                strCat(result, ",");

            // Keys are always strings in the output, so add starting quote and colon.
            strCatFmt(result, "\"%s\":", strPtr(key));

            // NULL value
            if (value == NULL)
                strCat(result, NULL_Z);
            else
            {
                switch (varType(value))
                {
                    case varTypeKeyValue:
                    {
                        strCat(result, strPtr(jsonFromKvInternal(kvDup(varKv(value)))));
                        break;
                    }

                    case varTypeVariantList:
                    {
                        // If the array is empty, then do not add formatting, else process the array.
                        if (varVarLst(value) == NULL)
                            strCat(result, NULL_Z);
                        else if (varLstSize(varVarLst(value)) == 0)
                            strCat(result, "[]");
                        else
                        {
                            strCat(result, "[");

                            for (unsigned int arrayIdx = 0; arrayIdx < varLstSize(varVarLst(value)); arrayIdx++)
                            {
                                Variant *arrayValue = varLstGet(varVarLst(value), arrayIdx);

                                // If going to add another element, add a comma
                                if (arrayIdx > 0)
                                    strCat(result, ",");

                                // If array value is null
                                if (arrayValue == NULL)
                                {
                                    strCat(result, NULL_Z);
                                }
                                // If the type is a string, add leading and trailing double quotes
                                else if (varType(arrayValue) == varTypeString)
                                {
                                    jsonFromStrInternal(result, varStr(arrayValue));
                                }
                                else if (varType(arrayValue) == varTypeKeyValue)
                                {
                                    strCat(result, strPtr(jsonFromKvInternal(kvDup(varKv(arrayValue)))));
                                }
                                else if (varType(arrayValue) == varTypeVariantList)
                                {
                                    strCat(result, strPtr(jsonFromVar(arrayValue)));
                                }
                                // Numeric, Boolean or other type
                                else
                                    strCat(result, strPtr(varStrForce(arrayValue)));
                            }

                            strCat(result, "]");
                        }

                        break;
                    }

                    // String
                    case varTypeString:
                    {
                        jsonFromStrInternal(result, varStr(value));
                        break;
                    }

                    default:
                    {
                        strCat(result, strPtr(varStrForce(value)));
                        break;
                    }
                }
            }
        }

        result = strCat(result, "}");
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_TEST_RETURN(result);
}

/***********************************************************************************************************************************
Currently this function is only intended to convert the limited types that are included in info files.  More types will be added as
needed.  Since this function is only intended to read internally-generated JSON it is assumed to be well-formed with no extraneous
whitespace.
***********************************************************************************************************************************/
String *
jsonFromKv(const KeyValue *kv)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(KEY_VALUE, kv);
    FUNCTION_LOG_END();

    ASSERT(kv != NULL);

    String *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        String *jsonStr = jsonFromKvInternal(kv);

        // Duplicate the string into the prior context
        MEM_CONTEXT_PRIOR_BEGIN()
        {
            result = strDup(jsonStr);
        }
        MEM_CONTEXT_PRIOR_END();
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(STRING, result);
}

/***********************************************************************************************************************************
Currently this function is only intended to convert the limited types that are included in info files.  More types will be added as
needed.
***********************************************************************************************************************************/
String *
jsonFromVar(const Variant *var)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(VARIANT, var);
    FUNCTION_LOG_END();

    String *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        String *jsonStr = strNew("");

        // If VariantList then process each item in the array. Currently the list must be KeyValue types.
        if (var == NULL)
        {
            strCat(jsonStr, strPtr(NULL_STR));
        }
        else if (varType(var) == varTypeBool)
        {
            strCat(jsonStr, strPtr(jsonFromBool(varBool(var))));
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
                strCat(jsonStr, "[");

                // Currently only KeyValue and String lists are supported
                for (unsigned int vlIdx = 0; vlIdx < varLstSize(vl); vlIdx++)
                {
                    // If going to add another key, append a comma
                    if (vlIdx > 0)
                        strCat(jsonStr, ",");

                    Variant *varSub = varLstGet(vl, vlIdx);

                    if (varSub == NULL)
                    {
                        strCat(jsonStr, NULL_Z);
                    }
                    else if (varType(varSub) == varTypeBool)
                    {
                        strCat(jsonStr, strPtr(jsonFromBool(varBool(varSub))));
                    }
                    else if (varType(varSub) == varTypeKeyValue)
                    {
                        strCat(jsonStr, strPtr(jsonFromKvInternal(varKv(varSub))));
                    }
                    else if (varType(varSub) == varTypeVariantList)
                    {
                        strCat(jsonStr, strPtr(jsonFromVar(varSub)));
                    }
                    else if (varType(varSub) == varTypeInt)
                    {
                        strCat(jsonStr, strPtr(jsonFromInt(varInt(varSub))));
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

                // Close the array
                strCat(jsonStr, "]");
            }
            // Else empty array
            else
                strCat(jsonStr, "[]");
        }
        else if (varType(var) == varTypeKeyValue)
        {
            strCat(jsonStr, strPtr(jsonFromKvInternal(varKv(var))));
        }
        else
            THROW(JsonFormatError, "variant type is invalid");

        // Duplicate the string into the prior context
        MEM_CONTEXT_PRIOR_BEGIN()
        {
            result = strDup(jsonStr);
        }
        MEM_CONTEXT_PRIOR_END();
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(STRING, result);
}
