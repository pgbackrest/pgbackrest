/***********************************************************************************************************************************
Convert JSON to/from KeyValue
***********************************************************************************************************************************/
#include "build.auto.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "common/debug.h"
#include "common/io/io.h"
#include "common/log.h"
#include "common/type/json.h"

/***********************************************************************************************************************************
Object types
***********************************************************************************************************************************/
typedef struct JsonReadStack
{
    JsonType type;                                                  // Container type
    bool first;                                                     // First element has been read
} JsonReadStack;

struct JsonRead
{
    const char *json;                                               // JSON string

    List *stack;                                                    // Stack of object/array tags
    bool key;                                                       // Was a key read for an object value?

    bool complete;                                                  // JSON read is complete
};

typedef struct JsonWriteStack
{
    JsonType type;                                                  // Container type
    bool first;                                                     // First element has been written
    String *keyLast;                                                // Last key set for an object value
} JsonWriteStack;

struct JsonWrite
{
    String *string;                                                 // JSON string

    List *stack;                                                    // Stack of object/array tags
    bool key;                                                       // Is a key set for an object value?

    bool complete;                                                  // JSON write is complete
};

/**********************************************************************************************************************************/
JsonRead *
jsonReadNew(const String *const string)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, string);
    FUNCTION_TEST_END();

    JsonRead *this = NULL;

    OBJ_NEW_BEGIN(JsonRead)
    {
        this = OBJ_NEW_ALLOC();

        *this = (JsonRead)
        {
            .json = strZ(string),
        };
    }
    OBJ_NEW_END();

    FUNCTION_TEST_RETURN(this);
}

/***********************************************************************************************************************************
Consume whitespace
***********************************************************************************************************************************/
static void
jsonReadConsumeWhiteSpace(JsonRead *const this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(JSON_READ, this);
    FUNCTION_TEST_END();

    bool whitespace = true;

    do
    {
        switch (*this->json)
        {
            case ' ':
            case '\t':
            case '\n':
            case '\r':
                this->json++;
                break;

            default:
                whitespace = false;
                break;
        }
    }
    while (whitespace);

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
JsonType
jsonReadTypeNext(JsonRead *const this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(JSON_READ, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    jsonReadConsumeWhiteSpace(this);

    // There should be some data
    if (*this->json == '\0')
        THROW(JsonFormatError, "expected data");

    // Determine data type
    JsonType result;

    switch (*this->json)
    {
        // String
        case '"':
            result = jsonTypeString;
            // result = varNewStr(jsonToStrInternal(json, jsonPos));
            break;

        // Integer
        case '-':
        case '0' ... '9':
            result = jsonTypeNumber;
            // result = jsonToNumberInternal(json, jsonPos);
            break;

        // Boolean
        case 't':
        case 'f':
            result = jsonTypeBool;
            // result = varNewBool(jsonToBoolInternal(json, jsonPos));
            break;

        // Null
        case 'n':
        {
            result = jsonTypeNull;
            // if (strncmp(json + *jsonPos, NULL_Z, 4) == 0)
            //     *jsonPos += 4;
            // else
            //     THROW_FMT(JsonFormatError, "expected null at '%s'", json + *jsonPos);

            break;
        }

        // Array
        case '[':
            result = jsonTypeArrayBegin;
            // result = varNewVarLst(jsonToVarLstInternal(json, jsonPos));
            break;

        case ']':
            result = jsonTypeArrayEnd;
            // result = varNewVarLst(jsonToVarLstInternal(json, jsonPos));
            break;

        // Object
        case '{':
            result = jsonTypeObjectBegin;
            // result = varNewKv(jsonToKvInternal(json, jsonPos));
            break;

        case '}':
            result = jsonTypeObjectEnd;
            // result = varNewKv(jsonToKvInternal(json, jsonPos));
            break;

        // Invalid type
        default:
            THROW_FMT(JsonFormatError, "invalid type at '%s'", this->json);
            break;
    }

    FUNCTION_TEST_RETURN(result);
}

static JsonType
jsonReadTypeNextIgnoreComma(JsonRead *const this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(JSON_READ, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    jsonReadConsumeWhiteSpace(this);

    // !!!
    if (*this->json != ',')
        FUNCTION_TEST_RETURN(jsonReadTypeNext(this));

    const char *const jsonBeforeComma = this->json;
    this->json++;

    const JsonType result = jsonReadTypeNext(this);
    this->json = jsonBeforeComma;

    FUNCTION_TEST_RETURN(result);
}

/***********************************************************************************************************************************
Push/Pop a type on/off the stack
***********************************************************************************************************************************/
static void
jsonReadPush(JsonRead *const this, const JsonType type, const bool key)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(JSON_READ, this);
        FUNCTION_TEST_PARAM(ENUM, type);
        FUNCTION_TEST_PARAM(BOOL, key);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    // Cannot read after complete
    if (this->complete)
        THROW(FormatError, "JSON read is complete");

    // !!!
    if (this->stack == NULL)
    {
        ASSERT(!key);

        if (jsonTypeScalar(type))
            this->complete = true;
        else
        {
            MEM_CONTEXT_BEGIN(objMemContext(this))
            {
                this->stack = lstNewP(sizeof(JsonReadStack));
            }
            MEM_CONTEXT_END();
        }
    }

    if (!this->complete)
    {
        ASSERT((jsonTypeContainer(type) && !key) || !lstEmpty(this->stack));

        if (!lstEmpty(this->stack))
        {
            JsonWriteStack *const item = lstGetLast(this->stack);

            if (key)
            {
                if (this->key)
                    THROW_FMT(FormatError, "key has already been defined at: %s", this->json);

                // if (item->keyLast != NULL && strCmp(key, item->keyLast) == -1)
                //     THROW_FMT(FormatError, "key '%s' is not after prior key '%s'", strZ(key), strZ(item->keyLast));

                // MEM_CONTEXT_BEGIN(lstMemContext(this->stack))
                // {
                //     strFree(item->keyLast);
                //     item->keyLast = strDup(key);
                // }
                // MEM_CONTEXT_END();

                this->key = true;
            }
            else
            {
                if (item->type == jsonTypeObjectBegin)
                {
                    if (!this->key)
                        THROW_FMT(JsonFormatError, "key has not been defined at: %s", this->json);

                    this->key = false;
                }
            }

            if (item->first && (item->type != jsonTypeObjectBegin || key))
            {
                jsonReadConsumeWhiteSpace(this);

                if (*this->json != ',')
                    THROW(FormatError, "missing comma");

                this->json++;
            }
            else
                item->first = true;
        }

        if (jsonTypeContainer(type))
            lstAdd(this->stack, &(JsonReadStack){.type = type});
    }

    if (jsonReadTypeNext(this) != type)
        THROW_FMT(FormatError, "expected %u but found '%s'", type, this->json);

    FUNCTION_TEST_RETURN_VOID();
}

static void
jsonReadPop(JsonRead *const this, const JsonType type)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(JSON_READ, this);
        FUNCTION_TEST_PARAM(ENUM, type);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(jsonTypeContainer(type));

    // Cannot read after complete
    if (this->complete)
        THROW(FormatError, "JSON read is complete");

    ASSERT(this->stack != NULL);
    ASSERT(!lstEmpty(this->stack));
    ASSERT_DECLARE(const JsonReadStack *const container = lstGetLast(this->stack));
    ASSERT(
        (container->type == jsonTypeArrayBegin && type == jsonTypeArrayEnd) ||
        (container->type == jsonTypeObjectBegin && type == jsonTypeObjectEnd));

    if (jsonReadTypeNext(this) != type)
        THROW_FMT(FormatError, "expected %c but found %c", type == jsonTypeArrayEnd ? ']' : '}', *this->json);

    // ASSERT(container->type != jsonTypeObjectBegin || this->key == false);

    if (type == jsonTypeObjectEnd)
    {
        // MEM_CONTEXT_BEGIN(lstMemContext(this->stack))
        // {
        //     strFree(((JsonWriteStack *)lstGetLast(this->stack))->keyLast);
        // }
        // MEM_CONTEXT_END();
    }

    lstRemoveLast(this->stack);

    if (lstEmpty(this->stack))
        this->complete = true;

    FUNCTION_TEST_RETURN_VOID();
}

/***********************************************************************************************************************************
Read JSON number
***********************************************************************************************************************************/
typedef enum
{
    jsonNumberTypeU64,
    jsonNumberTypeI64,
} JsonNumberType;

typedef struct JsonReadNumberResult
{
    JsonNumberType type;

    union
    {
        int64_t i64;
        uint64_t u64;
    } value;
} JsonReadNumberResult;

static JsonReadNumberResult
jsonReadNumber(JsonRead *const this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(JSON_READ, this);
    FUNCTION_TEST_END();

    // Consume the - when present
    const bool intSigned = *this->json == '-';

    // Consume all digits
    size_t digits = 0;

    while (isdigit(this->json[digits + intSigned]))
        digits++;

    // Invalid if no digits were found
    if (digits == 0)
        THROW(JsonFormatError, "no digits found");

    // Copy to buffer
    if (digits >= CVT_BASE10_BUFFER_SIZE)
        THROW(JsonFormatError, "invalid number");

    const size_t size = digits + intSigned;

    char working[CVT_BASE10_BUFFER_SIZE];
    strncpy(working, this->json, size);
    working[size] = '\0';

    this->json += size;

    // Return result
    if (intSigned)
        FUNCTION_TEST_RETURN((JsonReadNumberResult){.type = jsonNumberTypeI64, .value = {.i64 = cvtZToInt64(working)}});

    FUNCTION_TEST_RETURN((JsonReadNumberResult){.type = jsonNumberTypeU64, .value = {.u64 = cvtZToUInt64(working)}});
}

/***********************************************************************************************************************************
Read JSON string
***********************************************************************************************************************************/
static String *
jsonReadStrInternal(JsonRead *const this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(JSON_READ, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    String *const result = strNew();
    // if (json[*jsonPos] != '"')
    //     THROW_FMT(JsonFormatError, "expected '\"' at '%s'", json + *jsonPos);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Skip the beginning "
        this->json++;

        // Track portion of string with no escapes
        const char *noEscape = NULL;
        size_t noEscapeSize = 0;

        while (*this->json != '"')
        {
            if (*this->json == '\\')
            {
                // Copy portion of string without escapes
                if (noEscapeSize > 0)
                {
                    strCatZN(result, noEscape, noEscapeSize);
                    noEscapeSize = 0;
                }

                this->json++;

                switch (*this->json)
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

                    case 'u':
                    {
                        this->json++;

                        // We don't know how to decode anything except ASCII so fail if it looks like Unicode
                        if (strncmp(this->json, "00", 2) != 0)
                            THROW_FMT(JsonFormatError, "unable to decode '%.4s'", this->json);

                        // Decode char
                        this->json += 2;
                        strCatChr(result, (char)cvtZToUIntBase(strZ(strNewZN(this->json, 2)), 16));
                        this->json++;

                        break;
                    }

                    default:
                        THROW_FMT(JsonFormatError, "invalid escape character '%c'", *this->json);
                }
            }
            else
            {
                if (*this->json == '\0')
                    THROW(JsonFormatError, "expected '\"' but found null delimiter");

                // If escape string is zero size then start it
                if (noEscapeSize == 0)
                    noEscape = this->json;

                noEscapeSize++;
            }

            this->json++;
        };

        // Copy portion of string without escapes
        if (noEscapeSize > 0)
            strCatZN(result, noEscape, noEscapeSize);

        // Advance the character array pointer to the next element after the string
        this->json++;
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_TEST_RETURN(result);
}

/**********************************************************************************************************************************/
void
jsonReadArrayBegin(JsonRead *const this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(JSON_READ, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    jsonReadPush(this, jsonTypeArrayBegin, false);
    this->json++;

    FUNCTION_TEST_RETURN_VOID();
}

void
jsonReadArrayEnd(JsonRead *const this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(JSON_READ, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    jsonReadPop(this, jsonTypeArrayEnd);
    this->json++;

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
bool
jsonReadBool(JsonRead *const this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(JSON_READ, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    jsonReadPush(this, jsonTypeBool, false);

    if (strncmp(this->json, TRUE_Z, sizeof(TRUE_Z) - 1) == 0)
    {
        this->json += sizeof(TRUE_Z) - 1;

        FUNCTION_TEST_RETURN(true);
    }

    if (strncmp(this->json, FALSE_Z, sizeof(FALSE_Z) - 1) == 0)
    {
        this->json += sizeof(FALSE_Z) - 1;

        FUNCTION_TEST_RETURN(false);
    }

    THROW(JsonFormatError, "expected boolean");
}

/**********************************************************************************************************************************/
int
jsonReadInt(JsonRead *const this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(JSON_READ, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    jsonReadPush(this, jsonTypeNumber, false);

    JsonReadNumberResult number = jsonReadNumber(this);

    if (number.type == jsonNumberTypeU64)
    {
        if (number.value.u64 > INT_MAX)
            THROW(FormatError, "number is out of range for int");

        FUNCTION_TEST_RETURN((int)number.value.u64);
    }

    if (number.value.i64 > INT_MAX || number.value.i64 < INT_MIN)
        THROW(FormatError, "number is out of range for int");

    FUNCTION_TEST_RETURN((int)number.value.i64);
}

/**********************************************************************************************************************************/
int64_t
jsonReadInt64(JsonRead *const this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(JSON_READ, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    jsonReadPush(this, jsonTypeNumber, false);

    JsonReadNumberResult number = jsonReadNumber(this);

    if (number.type == jsonNumberTypeI64)
        FUNCTION_TEST_RETURN(number.value.i64);

    if (number.value.u64 > INT64_MAX)
        THROW(FormatError, "number is out of range for int64");

    FUNCTION_TEST_RETURN((int64_t)number.value.u64);
}

/**********************************************************************************************************************************/
static String *
jsonReadKeyInternal(JsonRead *const this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(JSON_READ, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    String *const result = jsonReadStrInternal(this);

    // Consume the : after the key
    jsonReadConsumeWhiteSpace(this);

    if (*this->json != ':')
        THROW_FMT(JsonFormatError, "expected : after key '%s' at: %s", strZ(result), this->json);

    this->json++;

    FUNCTION_TEST_RETURN(result);
}

String *
jsonReadKey(JsonRead *const this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(JSON_READ, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    jsonReadPush(this, jsonTypeString, true);

    FUNCTION_TEST_RETURN(jsonReadKeyInternal(this));
}

/**********************************************************************************************************************************/
bool
jsonReadKeyExpect(JsonRead *const this, const String *const key)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(JSON_READ, this);
        FUNCTION_TEST_PARAM(STRING, key);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(key != NULL);

    bool result = false;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        jsonReadConsumeWhiteSpace(this);

        JsonWriteStack *const item = lstGetLast(this->stack);
        ASSERT(item->type == jsonTypeObjectBegin);

        while (*this->json != '}')
        {
            const char *const jsonBeforeKey = this->json;
            const bool firstBeforeKey = item->first;

            jsonReadPush(this, jsonTypeString, true);

            const String *const keyNext = jsonReadKeyInternal(this);
            const int compare = strCmp(key, keyNext);

            if (compare == 0)
            {
                result = true;
                break;
            }
            else if (compare < 0)
            {
                this->json = jsonBeforeKey;
                this->key = false;
                item->first = firstBeforeKey;
                break;
            }

            jsonReadSkip(this);
            jsonReadConsumeWhiteSpace(this);
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_TEST_RETURN(result);
}

bool
jsonReadKeyExpectStrId(JsonRead *const this, const StringId key)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(JSON_READ, this);
        FUNCTION_TEST_PARAM(STRING_ID, key);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(key != 0);

    char buffer[STRID_MAX + 1];

    FUNCTION_TEST_RETURN(jsonReadKeyExpect(this, STR_SIZE(buffer, strIdToZ(key, buffer))));
}

bool
jsonReadKeyExpectZ(JsonRead *const this, const char *const key)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(JSON_READ, this);
        FUNCTION_TEST_PARAM(STRINGZ, key);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(key != NULL);

    FUNCTION_TEST_RETURN(jsonReadKeyExpect(this, STR(key)));
}

JsonRead *
jsonReadKeyRequire(JsonRead *const this, const String *const key)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(JSON_READ, this);
        FUNCTION_TEST_PARAM(STRING, key);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(key != NULL);

    if (!jsonReadKeyExpect(this, key))
        THROW_FMT(JsonFormatError, "required key '%s' not found", strZ(key));

    FUNCTION_TEST_RETURN(this);
}

JsonRead *
jsonReadKeyRequireStrId(JsonRead *const this, const StringId key)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(JSON_READ, this);
        FUNCTION_TEST_PARAM(STRING_ID, key);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(key != 0);

    char buffer[STRID_MAX + 1];

    FUNCTION_TEST_RETURN(jsonReadKeyRequire(this, STR_SIZE(buffer, strIdToZ(key, buffer))));
}

JsonRead *
jsonReadKeyRequireZ(JsonRead *const this, const char *const key)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(JSON_READ, this);
        FUNCTION_TEST_PARAM(STRINGZ, key);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(key != NULL);

    FUNCTION_TEST_RETURN(jsonReadKeyRequire(this, STR(key)));
}

/**********************************************************************************************************************************/
void
jsonReadNull(JsonRead *const this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(JSON_READ, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    jsonReadPush(this, jsonTypeNull, false);

    if (strncmp(this->json, NULL_Z, sizeof(NULL_Z) - 1) == 0)
        this->json += sizeof(NULL_Z) - 1;
    else
        THROW(JsonFormatError, "expected null");

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
void
jsonReadObjectBegin(JsonRead *const this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(JSON_READ, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    jsonReadPush(this, jsonTypeObjectBegin, false);
    this->json++;

    FUNCTION_TEST_RETURN_VOID();
}

void
jsonReadObjectEnd(JsonRead *const this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(JSON_READ, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    jsonReadPop(this, jsonTypeObjectEnd);
    this->json++;

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
static void
jsonReadSkipRecurse(JsonRead *const this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(JSON_READ, this);
    FUNCTION_TEST_END();

    switch (jsonReadTypeNextIgnoreComma(this))
    {
        case jsonTypeBool:
            jsonReadBool(this);
            break;

        case jsonTypeNull:
            jsonReadNull(this);
            break;

        case jsonTypeNumber:
            jsonReadPush(this, jsonTypeNumber, false);
            jsonReadNumber(this);
            break;

        case jsonTypeString:
            jsonReadPush(this, jsonTypeString, false);
            jsonReadStrInternal(this);
            break;

        case jsonTypeArrayBegin:
        {
            jsonReadArrayBegin(this);

            while (jsonReadTypeNextIgnoreComma(this) != jsonTypeArrayEnd)
                jsonReadSkipRecurse(this);

            jsonReadArrayEnd(this);

            break;
        }

        case jsonTypeObjectBegin:
        {
            jsonReadObjectBegin(this);

            while (jsonReadTypeNextIgnoreComma(this) != jsonTypeObjectEnd)
            {
                jsonReadKey(this);
                jsonReadSkipRecurse(this);
            }

            jsonReadObjectEnd(this);

            break;
        }

        default:
            THROW_FMT(JsonFormatError, "invalid container end at: %s", this->json);
    }

    FUNCTION_TEST_RETURN_VOID();
}

void
jsonReadSkip(JsonRead *const this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(JSON_READ, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        jsonReadSkipRecurse(this);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
String *
jsonReadStr(JsonRead *const this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(JSON_READ, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    jsonReadPush(this, jsonTypeString, false);

    FUNCTION_TEST_RETURN(jsonReadStrInternal(this));
}

/**********************************************************************************************************************************/
unsigned int
jsonReadUInt(JsonRead *const this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(JSON_READ, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    jsonReadPush(this, jsonTypeNumber, false);

    JsonReadNumberResult number = jsonReadNumber(this);

    if (number.type != jsonNumberTypeU64 || number.value.u64 > UINT_MAX)
        THROW(JsonFormatError, "number is out of range for uint");

    FUNCTION_TEST_RETURN((unsigned int)number.value.u64);
}

/**********************************************************************************************************************************/
static Variant *
jsonReadVarRecurse(JsonRead *const this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(JSON_READ, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    switch (jsonReadTypeNextIgnoreComma(this))
    {
        case jsonTypeBool:
            FUNCTION_TEST_RETURN(varNewBool(jsonReadBool(this)));

        case jsonTypeNull:
            jsonReadNull(this);
            FUNCTION_TEST_RETURN(NULL);

        case jsonTypeNumber:
        {
            jsonReadPush(this, jsonTypeNumber, false);

            JsonReadNumberResult number = jsonReadNumber(this);

            if (number.type == jsonNumberTypeU64)
                FUNCTION_TEST_RETURN(varNewUInt64(number.value.u64));

            FUNCTION_TEST_RETURN(varNewInt64(number.value.i64));
        }

        case jsonTypeString:
            jsonReadPush(this, jsonTypeString, false);
            FUNCTION_TEST_RETURN(varNewStr(jsonReadStrInternal(this)));

        case jsonTypeArrayBegin:
        {
            VariantList *const result = varLstNew();

            jsonReadArrayBegin(this);

            while (jsonReadTypeNextIgnoreComma(this) != jsonTypeArrayEnd)
                varLstAdd(result, jsonReadVarRecurse(this));

            jsonReadArrayEnd(this);

            FUNCTION_TEST_RETURN(varNewVarLst(result));
        }

        case jsonTypeObjectBegin:
        {
            KeyValue *const result = kvNew();

            jsonReadObjectBegin(this);

            while (jsonReadTypeNextIgnoreComma(this) != jsonTypeObjectEnd)
            {
                const String *const key = jsonReadKey(this);
                kvPut(result, varNewStr(key), jsonReadVarRecurse(this));
            }

            jsonReadObjectEnd(this);

            FUNCTION_TEST_RETURN(varNewKv(result));
        }

        default:
            THROW_FMT(JsonFormatError, "invalid container end at: %s", this->json);
    }
}

Variant *
jsonReadVar(JsonRead *const this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(JSON_READ, this);
    FUNCTION_TEST_END();

    Variant *const result = jsonReadVarRecurse(this);

    jsonReadConsumeWhiteSpace(this);

    FUNCTION_TEST_RETURN(result);
}

/**********************************************************************************************************************************/
uint64_t
jsonReadUInt64(JsonRead *const this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(JSON_READ, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    jsonReadPush(this, jsonTypeNumber, false);

    JsonReadNumberResult number = jsonReadNumber(this);

    if (number.type != jsonNumberTypeU64)
        THROW(JsonFormatError, "number is out of range for uint64");

    FUNCTION_TEST_RETURN(number.value.u64);
}

/**********************************************************************************************************************************/
Variant *
jsonToVar(const String *const json)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, json);
    FUNCTION_TEST_END();

    Variant *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        JsonRead *const read = jsonReadNew(json);

        MEM_CONTEXT_PRIOR_BEGIN()
        {
            result = jsonReadVar(read);
        }
        MEM_CONTEXT_PRIOR_END();
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_TEST_RETURN(result);
}

/**********************************************************************************************************************************/
void
jsonValidate(const String *const json)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, json);
    FUNCTION_TEST_END();

    MEM_CONTEXT_TEMP_BEGIN()
    {
        JsonRead *const read = jsonReadNew(json);

        MEM_CONTEXT_PRIOR_BEGIN()
        {
            jsonReadSkip(read);

            if (*read->json != '\0')
                THROW_FMT(JsonFormatError, "characters after JSON at: %s", read->json);
        }
        MEM_CONTEXT_PRIOR_END();
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
String *
jsonReadToLog(const JsonRead *const this)
{
    // !!! MAKE THIS REAL
    return strNewFmt("{json: %s}", this->json);
}

/**********************************************************************************************************************************/
JsonWrite *
jsonWriteNew(JsonWriteNewParam param)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, param.string);
    FUNCTION_TEST_END();

    JsonWrite *this = NULL;

    OBJ_NEW_BEGIN(JsonWrite)
    {
        this = OBJ_NEW_ALLOC();

        *this = (JsonWrite)
        {
            .string = param.string == NULL ? strNew() : param.string,
        };
    }
    OBJ_NEW_END();

    FUNCTION_TEST_RETURN(this);
}

/***********************************************************************************************************************************
Push/Pop a type on/off the stack
***********************************************************************************************************************************/
static void
jsonWritePush(JsonWrite *const this, const JsonType jsonType, const String *const key)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(JSON_WRITE, this);
        FUNCTION_TEST_PARAM(ENUM, jsonType);
        FUNCTION_TEST_PARAM(STRING, key);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    // Cannot write after complete
    if (this->complete)
        THROW(JsonFormatError, "JSON write is complete");

    // !!!
    if (this->stack == NULL)
    {
        ASSERT(key == NULL);

        if (jsonTypeScalar(jsonType))
            this->complete = true;
        else
        {
            MEM_CONTEXT_BEGIN(objMemContext(this))
            {
                this->stack = lstNewP(sizeof(JsonWriteStack));
            }
            MEM_CONTEXT_END();
        }
    }

    if (!this->complete)
    {
        ASSERT((jsonTypeContainer(jsonType) && key == NULL) || !lstEmpty(this->stack));

        if (!lstEmpty(this->stack))
        {
            JsonWriteStack *const item = lstGetLast(this->stack);

            if (key != NULL)
            {
                if (this->key)
                    THROW_FMT(JsonFormatError, "key has already been defined");

                if (item->keyLast != NULL && strCmp(key, item->keyLast) == -1)
                    THROW_FMT(JsonFormatError, "key '%s' is not after prior key '%s'", strZ(key), strZ(item->keyLast));

                MEM_CONTEXT_BEGIN(lstMemContext(this->stack))
                {
                    strFree(item->keyLast);
                    item->keyLast = strDup(key);
                }
                MEM_CONTEXT_END();

                this->key = true;
            }
            else
            {
                if (item->type == jsonTypeObjectBegin)
                {
                    if (!this->key)
                        THROW_FMT(JsonFormatError, "key has not been defined");

                    this->key = false;
                }
            }

            if (item->first && (item->type != jsonTypeObjectBegin || key != NULL))
                strCatChr(this->string, ',');
            else
                item->first = true;
        }

        if (jsonTypeContainer(jsonType))
            lstAdd(this->stack, &(JsonWriteStack){.type = jsonType});
    }

    FUNCTION_TEST_RETURN_VOID();
}

static void
jsonWritePop(JsonWrite *const this, const JsonType type)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(JSON_WRITE, this);
        FUNCTION_TEST_PARAM(ENUM, type);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(this->stack != NULL);
    ASSERT(!lstEmpty(this->stack));
    ASSERT(jsonTypeContainer(type));
    ASSERT_DECLARE(const JsonWriteStack *const container = lstGetLast(this->stack));
    ASSERT(container->type == type);
    ASSERT(container->type != jsonTypeObjectBegin || this->key == false);

    if (type == jsonTypeObjectBegin)
    {
        MEM_CONTEXT_BEGIN(lstMemContext(this->stack))
        {
            strFree(((JsonWriteStack *)lstGetLast(this->stack))->keyLast);
        }
        MEM_CONTEXT_END();
    }

    lstRemoveLast(this->stack);

    if (lstEmpty(this->stack))
        this->complete = true;

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
JsonWrite *
jsonWriteArrayBegin(JsonWrite *const this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(JSON_WRITE, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    jsonWritePush(this, jsonTypeArrayBegin, NULL);
    strCatChr(this->string, '[');

    FUNCTION_TEST_RETURN(this);
}

JsonWrite *
jsonWriteArrayEnd(JsonWrite *const this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(JSON_WRITE, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    jsonWritePop(this, jsonTypeArrayBegin);
    strCatChr(this->string, ']');

    FUNCTION_TEST_RETURN(this);
}

/**********************************************************************************************************************************/
JsonWrite *
jsonWriteBool(JsonWrite *const this, const bool value)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(JSON_WRITE, this);
        FUNCTION_TEST_PARAM(BOOL, value);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    jsonWritePush(this, jsonTypeBool, NULL);
    strCat(this->string, value ? TRUE_STR : FALSE_STR);

    FUNCTION_TEST_RETURN(this);
}

/**********************************************************************************************************************************/
JsonWrite *
jsonWriteInt(JsonWrite *const this, const int value)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(JSON_WRITE, this);
        FUNCTION_TEST_PARAM(INT, value);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    jsonWritePush(this, jsonTypeNumber, NULL);

    char working[CVT_BASE10_BUFFER_SIZE];
    strCatZN(this->string, working, cvtIntToZ(value, working, sizeof(working)));

    FUNCTION_TEST_RETURN(this);
}

/**********************************************************************************************************************************/
JsonWrite *
jsonWriteInt64(JsonWrite *const this, const int64_t value)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(JSON_WRITE, this);
        FUNCTION_TEST_PARAM(INT64, value);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    jsonWritePush(this, jsonTypeNumber, NULL);

    char working[CVT_BASE10_BUFFER_SIZE];
    strCatZN(this->string, working, cvtInt64ToZ(value, working, sizeof(working)));

    FUNCTION_TEST_RETURN(this);
}

/**********************************************************************************************************************************/
JsonWrite *
jsonWriteJson(JsonWrite *const this, const String *const value)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(JSON_WRITE, this);
        FUNCTION_TEST_PARAM(STRING, value);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    jsonWritePush(this, jsonTypeString, NULL);

    if (value == NULL)
        strCat(this->string, NULL_STR);
    else
        strCat(this->string, value);

    FUNCTION_TEST_RETURN(this);
}

/**********************************************************************************************************************************/
// Write String to JSON
static void
jsonWriteStrInternal(JsonWrite *const this, const String *const value)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(JSON_WRITE, this);
        FUNCTION_TEST_PARAM(STRING, value);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(value != NULL);

    strCatChr(this->string, '"');

    // Track portion of string with no escapes
    const char *valuePtr = strZ(value);
    const char *noEscape = NULL;
    size_t noEscapeSize = 0;

    for (unsigned int valueIdx = 0; valueIdx < strSize(value); valueIdx++)
    {
        switch (*valuePtr)
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
                    strCatZN(this->string, noEscape, noEscapeSize);
                    noEscapeSize = 0;
                }

                switch (*valuePtr)
                {
                    case '"':
                        strCatZ(this->string, "\\\"");
                        break;

                    case '\\':
                        strCatZ(this->string, "\\\\");
                        break;

                    case '\n':
                        strCatZ(this->string, "\\n");
                        break;

                    case '\r':
                        strCatZ(this->string, "\\r");
                        break;

                    case '\t':
                        strCatZ(this->string, "\\t");
                        break;

                    case '\b':
                        strCatZ(this->string, "\\b");
                        break;

                    case '\f':
                        strCatZ(this->string, "\\f");
                        break;
                }

                break;
            }

            default:
            {
                // If escape string is zero size then start it
                if (noEscapeSize == 0)
                    noEscape = valuePtr;

                noEscapeSize++;
                break;
            }
        }

        valuePtr++;
    }

    // Copy portion of string without escapes
    if (noEscapeSize > 0)
        strCatZN(this->string, noEscape, noEscapeSize);

    strCatChr(this->string, '"');

    FUNCTION_TEST_RETURN_VOID();
}

JsonWrite *
jsonWriteKey(JsonWrite *const this, const String *const key)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(JSON_WRITE, this);
        FUNCTION_TEST_PARAM(STRING, key);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(key != NULL);

    jsonWritePush(this, jsonTypeString, key);

    jsonWriteStrInternal(this, key);
    strCatChr(this->string, ':');

    FUNCTION_TEST_RETURN(this);
}

JsonWrite *
jsonWriteKeyStrId(JsonWrite *const this, const StringId key)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(JSON_WRITE, this);
        FUNCTION_TEST_PARAM(STRING_ID, key);
    FUNCTION_TEST_END();

    char buffer[STRID_MAX + 4];
    size_t size = strIdToZ(key, buffer + 1);

    jsonWritePush(this, jsonTypeString, STR_SIZE(buffer + 1, size));

    buffer[0] = '"';
    buffer[++size] = '"';
    buffer[++size] = ':';

    strCatZN(this->string, buffer, size + 1);

    FUNCTION_TEST_RETURN(this);
}

JsonWrite *
jsonWriteKeyZ(JsonWrite *const this, const char *const key)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(JSON_WRITE, this);
        FUNCTION_TEST_PARAM(STRINGZ, key);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(key != NULL);

    FUNCTION_TEST_RETURN(jsonWriteKey(this, STR(key)));
}

/**********************************************************************************************************************************/
JsonWrite *
jsonWriteObjectBegin(JsonWrite *const this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(JSON_WRITE, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    jsonWritePush(this, jsonTypeObjectBegin, NULL);
    strCatChr(this->string, '{');

    FUNCTION_TEST_RETURN(this);
}

JsonWrite *
jsonWriteObjectEnd(JsonWrite *const this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(JSON_WRITE, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    jsonWritePop(this, jsonTypeObjectBegin);
    strCatChr(this->string, '}');

    FUNCTION_TEST_RETURN(this);
}

/**********************************************************************************************************************************/
JsonWrite *
jsonWriteStr(JsonWrite *const this, const String *const value)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(JSON_WRITE, this);
        FUNCTION_TEST_PARAM(STRING, value);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(value != NULL);

    jsonWritePush(this, jsonTypeString, NULL);

    if (value == NULL)
        strCat(this->string, NULL_STR);
    else
        jsonWriteStrInternal(this, value);

    FUNCTION_TEST_RETURN(this);
}

/**********************************************************************************************************************************/
JsonWrite *
jsonWriteStrFmt(JsonWrite *const this, const char *const format, ...)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(JSON_WRITE, this);
        FUNCTION_TEST_PARAM(STRINGZ, format);
    FUNCTION_TEST_END();

    jsonWritePush(this, jsonTypeString, NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Determine how long the allocated string needs to be
        va_list argumentList;
        va_start(argumentList, format);
        const size_t size = (size_t)vsnprintf(NULL, 0, format, argumentList);
        va_end(argumentList);

        // Format string
        va_start(argumentList, format);
        char *const buffer = memNew(size + 1);
        vsnprintf(buffer, size + 1, format, argumentList);
        va_end(argumentList);

        // Write as JSON
        jsonWriteStrInternal(this, STR_SIZE(buffer, size));
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_TEST_RETURN(this);
}

/**********************************************************************************************************************************/
JsonWrite *
jsonWriteStrLst(JsonWrite *const this, const StringList *const value)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(JSON_WRITE, this);
        FUNCTION_TEST_PARAM(STRING_LIST, value);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(value != NULL);

    jsonWriteArrayBegin(this);

    for (unsigned int valueIdx = 0; valueIdx < strLstSize(value); valueIdx++)
        jsonWriteStr(this, strLstGet(value, valueIdx));

    jsonWriteArrayEnd(this);

    FUNCTION_TEST_RETURN(this);
}

/**********************************************************************************************************************************/
JsonWrite *
jsonWriteUInt(JsonWrite *const this, const unsigned int value)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(JSON_WRITE, this);
        FUNCTION_TEST_PARAM(UINT, value);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    jsonWritePush(this, jsonTypeNumber, NULL);

    char working[CVT_BASE10_BUFFER_SIZE];
    strCatZN(this->string, working, cvtUIntToZ(value, working, sizeof(working)));

    FUNCTION_TEST_RETURN(this);
}

/**********************************************************************************************************************************/
JsonWrite *
jsonWriteUInt64(JsonWrite *const this, const uint64_t value)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(JSON_WRITE, this);
        FUNCTION_TEST_PARAM(UINT64, value);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    jsonWritePush(this, jsonTypeNumber, NULL);

    char working[CVT_BASE10_BUFFER_SIZE];
    strCatZN(this->string, working, cvtUInt64ToZ(value, working, sizeof(working)));

    FUNCTION_TEST_RETURN(this);
}

/**********************************************************************************************************************************/
// Write a Variant to JSON
static void
jsonWriteVarInternal(JsonWrite *const this, const Variant *const value)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(JSON_WRITE, this);
        FUNCTION_TEST_PARAM(VARIANT, value);
    FUNCTION_TEST_END();

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // If VariantList then process each item in the array. Currently the list must be KeyValue types.
        if (value == NULL)
            strCat(this->string, NULL_STR);
        else
        {
            switch (varType(value))
            {
                case varTypeBool:
                case varTypeInt:
                case varTypeInt64:
                case varTypeUInt:
                case varTypeUInt64:
                    strCat(this->string, varStrForce(value));
                    break;

                case varTypeKeyValue:
                {
                    const KeyValue *const keyValue = varKv(value);

                    if (keyValue == NULL)
                        strCat(this->string, NULL_STR);
                    else
                    {
                        const StringList *const keyList = strLstSort(strLstNewVarLst(kvKeyList(keyValue)), sortOrderAsc);

                        strCatChr(this->string, '{');

                        for (unsigned int keyListIdx = 0; keyListIdx < strLstSize(keyList); keyListIdx++)
                        {
                            // Add common after first key/value
                            if (keyListIdx > 0)
                                strCatChr(this->string, ',');

                            // Write key
                            const String *const key = strLstGet(keyList, keyListIdx);

                            jsonWriteStrInternal(this, key);
                            strCatChr(this->string, ':');

                            // Write value
                            jsonWriteVarInternal(this, kvGet(keyValue, VARSTR(key)));
                        }

                        strCatChr(this->string, '}');
                    }

                    break;
                }

                case varTypeString:
                {
                    if (varStr(value) == NULL)
                        strCat(this->string, NULL_STR);
                    else
                        jsonWriteStrInternal(this, varStr(value));

                    break;
                }

                default:
                {
                    ASSERT(varType(value) == varTypeVariantList);

                    const VariantList *const valueList = varVarLst(value);

                    if (valueList == NULL)
                        strCat(this->string, NULL_STR);
                    else
                    {
                        strCatChr(this->string, '[');

                        for (unsigned int valueListIdx = 0; valueListIdx < varLstSize(valueList); valueListIdx++)
                        {
                            // Add common after first value
                            if (valueListIdx > 0)
                                strCatChr(this->string, ',');

                            jsonWriteVarInternal(this, varLstGet(valueList, valueListIdx));
                        }

                        strCatChr(this->string, ']');
                    }
                }
            }
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_TEST_RETURN_VOID();
}

JsonWrite *
jsonWriteVar(JsonWrite *const this, const Variant *const value)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(JSON_WRITE, this);
        FUNCTION_TEST_PARAM(VARIANT, value);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    jsonWritePush(this, jsonTypeString, NULL);
    jsonWriteVarInternal(this, value);

    FUNCTION_TEST_RETURN(this);
}

/**********************************************************************************************************************************/
JsonWrite *
jsonWriteZ(JsonWrite *const this, const char *const value)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(JSON_WRITE, this);
        FUNCTION_TEST_PARAM(STRINGZ, value);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    jsonWritePush(this, jsonTypeString, NULL);

    if (value == NULL)
        strCat(this->string, NULL_STR);
    else
        jsonWriteStrInternal(this, STR(value));

    FUNCTION_TEST_RETURN(this);
}

/**********************************************************************************************************************************/
const String *
jsonWriteResult(JsonWrite *const this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(JSON_WRITE, this);
    FUNCTION_TEST_END();

    if (this != NULL)
    {
        ASSERT(this->complete);
        FUNCTION_TEST_RETURN(this->string);
    }

    FUNCTION_TEST_RETURN(NULL);
}

/**********************************************************************************************************************************/
String *
jsonFromVar(const Variant *const value)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(VARIANT, value);
    FUNCTION_TEST_END();

    String *const result = strNew();

    MEM_CONTEXT_TEMP_BEGIN()
    {
        jsonWriteVar(jsonWriteNewP(.string = result), value);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_TEST_RETURN(result);
}

/**********************************************************************************************************************************/
String *
jsonWriteToLog(const JsonWrite *const this)
{
    return strNewFmt("{depth: %u}", this->stack != NULL ? lstSize(this->stack) : (unsigned int)(this->complete ? 1 : 0));
}
