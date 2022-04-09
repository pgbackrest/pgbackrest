/***********************************************************************************************************************************
Convert JSON to/from KeyValue
***********************************************************************************************************************************/
#include "build.auto.h"

#include <ctype.h>
#include <string.h>

#include "common/debug.h"
#include "common/io/io.h"
#include "common/log.h"
#include "common/type/json.h"

/***********************************************************************************************************************************
Prototypes
***********************************************************************************************************************************/
static Variant *jsonToVarInternal(const char *json, unsigned int *jsonPos);

/***********************************************************************************************************************************
JSON types
***********************************************************************************************************************************/
typedef enum
{
    jsonTypeArray = 0,
    jsonTypeObject = 1,
    jsonTypeString,
    jsonTypeNumber,
    jsonTypeBool,
} JsonType;

__attribute__((always_inline)) static inline bool
jsonTypeContainer(const JsonType jsonType)
{
    return jsonType <= jsonTypeObject;
}

__attribute__((always_inline)) static inline bool
jsonTypeScalar(const JsonType jsonType)
{
    return jsonType > jsonTypeObject;
}

/***********************************************************************************************************************************
Object types
***********************************************************************************************************************************/
typedef struct JsonStack
{
    JsonType type;                                                  // Container type
    bool first;                                                     // First element added
    bool key;                                                       // Is a key set for an object value
    String *keyLast;                                                // Last key set for an object value
} JsonStack;

struct JsonWrite
{
    IoWrite *write;                                                 // Write pack to
    Buffer *buffer;                                                 // Buffer to contain write data

    bool complete;                                                  // JSON is complete an nothing more can be written
    List *stack;                                                    // Stack of object/array tags
};

/**********************************************************************************************************************************/
// Helper to create common data
static JsonWrite *
jsonWriteNewInternal(void)
{
    FUNCTION_TEST_VOID();

    JsonWrite *this = NULL;

    OBJ_NEW_BEGIN(JsonWrite)
    {
        this = OBJ_NEW_ALLOC();

        *this = (JsonWrite)
        {
            .complete = false,
        };
    }
    OBJ_NEW_END();

    FUNCTION_TEST_RETURN(this);
}

JsonWrite *
jsonWriteNew(const JsonWriteNewParam param)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(SIZE, param.size);
    FUNCTION_TEST_END();

    JsonWrite *this = jsonWriteNewInternal();

    MEM_CONTEXT_BEGIN(objMemContext(this))
    {
        this->buffer = bufNew(param.size == 0 ? JSON_EXTRA_MIN : param.size);
    }
    MEM_CONTEXT_END();

    FUNCTION_TEST_RETURN(this);
}

JsonWrite *
jsonWriteNewIo(IoWrite *const write)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(IO_WRITE, write);
    FUNCTION_TEST_END();

    ASSERT(write != NULL);

    JsonWrite *this = jsonWriteNewInternal();

    MEM_CONTEXT_BEGIN(objMemContext(this))
    {
        this->write = write;
        this->buffer = bufNew(ioBufferSize());
    }
    MEM_CONTEXT_END();

    FUNCTION_TEST_RETURN(this);
}

/***********************************************************************************************************************************
Write to io or buffer
***********************************************************************************************************************************/
static void
jsonWriteBuffer(JsonWrite *const this, const Buffer *const buffer)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(JSON_WRITE, this);
        FUNCTION_TEST_PARAM(BUFFER, buffer);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    // If writing directly to a buffer
    if (this->write == NULL)
    {
        // Add space in the buffer to write and add extra space so future writes won't always need to resize the buffer
        if (bufRemains(this->buffer) < bufUsed(buffer))
            bufResize(this->buffer, (bufSizeAlloc(this->buffer) + bufUsed(buffer)) + PACK_EXTRA_MIN);

        // Write to the buffer
        bufCat(this->buffer, buffer);
    }
    // Else writing to io
    else
    {
        // If there's enough space to write to the internal buffer then do that
        if (bufRemains(this->buffer) >= bufUsed(buffer))
            bufCat(this->buffer, buffer);
        else
        {
            // Flush the internal buffer if it has data
            if (!bufEmpty(this->buffer))
            {
                ioWrite(this->write, this->buffer);
                bufUsedZero(this->buffer);
            }

            // If there's enough space to write to the internal buffer then do that
            if (bufRemains(this->buffer) >= bufUsed(buffer))
            {
                bufCat(this->buffer, buffer);
            }
            // Else write directly to io
            else
                ioWrite(this->write, buffer);
        }
    }

    FUNCTION_TEST_RETURN_VOID();
}

/***********************************************************************************************************************************
Push/Pop a type on/off the stack
***********************************************************************************************************************************/
static void
jsonTypePush(JsonWrite *const this, const JsonType jsonType, const String *const key)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(JSON_WRITE, this);
        FUNCTION_TEST_PARAM(ENUM, jsonType);
        FUNCTION_TEST_PARAM(STRING, key);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    // Cannot build on a scalar
    if (this->complete)
        THROW(FormatError, "JSON is complete and nothing more may be added");

    // !!!
    if (this->stack == NULL)
    {
        ASSERT(key == false);

        if (jsonTypeScalar(jsonType))
            this->complete = true;
        else
        {
            MEM_CONTEXT_BEGIN(objMemContext(this))
            {
                this->stack = lstNewP(sizeof(JsonStack));
            }
            MEM_CONTEXT_END();
        }
    }

    if (!this->complete)
    {
        ASSERT((jsonTypeContainer(jsonType) && key == NULL) || !lstEmpty(this->stack));

        if (!lstEmpty(this->stack))
        {
            JsonStack *const item = lstGetLast(this->stack);

            if (key != NULL)
            {
                if (item->key)
                    THROW_FMT(FormatError, "key has already been defined");

                if (item->keyLast != NULL && strCmp(key, item->keyLast) == -1)
                    THROW_FMT(FormatError, "key '%s' is not after prior key '%s'", strZ(key), strZ(item->keyLast));

                MEM_CONTEXT_BEGIN(lstMemContext(this->stack))
                {
                    strFree(item->keyLast);
                    item->keyLast = strDup(key);
                }
                MEM_CONTEXT_END();

                item->key = true;
            }
            else
            {
                if (item->type == jsonTypeObject)
                {
                    if (!item->key)
                        THROW_FMT(FormatError, "key has not been defined");

                    item->key = false;
                }
            }

            if (item->first && (item->type != jsonTypeObject || key != NULL))
                jsonWriteBuffer(this, COMMA_BUF);
            else
                item->first = true;
        }

        if (jsonTypeContainer(jsonType))
            lstAdd(this->stack, &(JsonStack){.type = jsonType});
    }

    FUNCTION_TEST_RETURN_VOID();
}

static void
jsonTypePop(JsonWrite *const this, const JsonType type)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(JSON_WRITE, this);
        FUNCTION_TEST_PARAM(ENUM, type);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(this->stack != NULL);
    ASSERT(!lstEmpty(this->stack));
    ASSERT(jsonTypeContainer(type));
    ASSERT_DECLARE(const JsonStack *const container = lstGetLast(this->stack));
    ASSERT(container->type == type);
    ASSERT(container->type != jsonTypeObject || container->key == false);

    if (type == jsonTypeObject)
    {
        MEM_CONTEXT_BEGIN(lstMemContext(this->stack))
        {
            strFree(((JsonStack *)lstGetLast(this->stack))->keyLast);
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

    jsonTypePush(this, jsonTypeArray, false);
    jsonWriteBuffer(this, BRACKETL_BUF);

    FUNCTION_TEST_RETURN(this);
}

JsonWrite *
jsonWriteArrayEnd(JsonWrite *const this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(JSON_WRITE, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    jsonTypePop(this, jsonTypeArray);
    jsonWriteBuffer(this, BRACKETR_BUF);

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

    jsonTypePush(this, jsonTypeBool, false);
    jsonWriteBuffer(this, value ? BUFSTRDEF("true") : BUFSTRDEF("false"));

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

    jsonTypePush(this, jsonTypeNumber, false);

    char working[CVT_BASE10_BUFFER_SIZE];
    cvtIntToZ(value, working, sizeof(working));

    jsonWriteBuffer(this, BUFSTRZ(working));

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

    jsonTypePush(this, jsonTypeNumber, false);

    char working[CVT_BASE10_BUFFER_SIZE];
    cvtInt64ToZ(value, working, sizeof(working));

    jsonWriteBuffer(this, BUFSTRZ(working));

    FUNCTION_TEST_RETURN(this);
}

/**********************************************************************************************************************************/
static void
jsonWriteStrInternal(JsonWrite *const this, const String *const value)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(JSON_WRITE, this);
        FUNCTION_TEST_PARAM(STRING, value);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(value != NULL);

    jsonWriteBuffer(this, QUOTED_BUF);

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
                    jsonWriteBuffer(this, BUF(noEscape, noEscapeSize));
                    noEscapeSize = 0;
                }

                switch (*valuePtr)
                {
                    case '"':
                        jsonWriteBuffer(this, BUFSTRDEF("\\\""));
                        break;

                    case '\\':
                        jsonWriteBuffer(this, BUFSTRDEF("\\\\"));
                        break;

                    case '\n':
                        jsonWriteBuffer(this, BUFSTRDEF("\\n"));
                        break;

                    case '\r':
                        jsonWriteBuffer(this, BUFSTRDEF("\\r"));
                        break;

                    case '\t':
                        jsonWriteBuffer(this, BUFSTRDEF("\\t"));
                        break;

                    case '\b':
                        jsonWriteBuffer(this, BUFSTRDEF("\\b"));
                        break;

                    case '\f':
                        jsonWriteBuffer(this, BUFSTRDEF("\\f"));
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
        jsonWriteBuffer(this, BUF(noEscape, noEscapeSize));

    jsonWriteBuffer(this, QUOTED_BUF);

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

    jsonTypePush(this, jsonTypeString, key);

    jsonWriteStrInternal(this, key);
    jsonWriteBuffer(this, BUFSTRDEF(":"));

    FUNCTION_TEST_RETURN(this);
}

JsonWrite *
jsonWriteKeyZ(JsonWrite *const this, const char *const key)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(JSON_WRITE, this);
        FUNCTION_TEST_PARAM(STRING, key);
    FUNCTION_TEST_END();

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

    jsonTypePush(this, jsonTypeObject, false);
    jsonWriteBuffer(this, BRACEL_BUF);

    FUNCTION_TEST_RETURN(this);
}

JsonWrite *
jsonWriteObjectEnd(JsonWrite *const this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(JSON_WRITE, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    jsonTypePop(this, jsonTypeObject);
    jsonWriteBuffer(this, BRACER_BUF);

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

    jsonTypePush(this, jsonTypeString, false);

    if (value == NULL)
        jsonWriteBuffer(this, BUFSTRDEF("null"));
    else
        jsonWriteStrInternal(this, value);

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

    jsonTypePush(this, jsonTypeNumber, false);

    char working[CVT_BASE10_BUFFER_SIZE];
    cvtUIntToZ(value, working, sizeof(working));

    jsonWriteBuffer(this, BUFSTRZ(working));

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

    jsonTypePush(this, jsonTypeNumber, false);

    char working[CVT_BASE10_BUFFER_SIZE];
    cvtUInt64ToZ(value, working, sizeof(working));

    jsonWriteBuffer(this, BUFSTRZ(working));

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

    jsonTypePush(this, jsonTypeString, false);

    if (value == NULL)
        jsonWriteBuffer(this, BUFSTRDEF("null"));
    else
        jsonWriteStrInternal(this, STR(value));

    FUNCTION_TEST_RETURN(this);
}

/**********************************************************************************************************************************/
const Buffer *
jsonWriteResult(JsonWrite *const this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(JSON_WRITE, this);
    FUNCTION_TEST_END();

    Buffer *result = NULL;

    if (this != NULL)
    {
        // ASSERT(this->tagStack.top == NULL);

        result = this->buffer;
    }

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
    jsonConsumeWhiteSpace(strZ(json), &jsonPos);

    bool result = jsonToBoolInternal(strZ(json), &jsonPos);

    jsonConsumeWhiteSpace(strZ(json), &jsonPos);

    if (jsonPos != strSize(json))
        THROW_FMT(JsonFormatError, "unexpected characters after boolean at '%s'", strZ(json) + jsonPos);

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
        String *resultStr = strNewZN(json + beginPos, *jsonPos - beginPos);

        // Convert the string to a integer variant
        MEM_CONTEXT_PRIOR_BEGIN()
        {
            if (intSigned)
                result = varNewInt64(cvtZToInt64(strZ(resultStr)));
            else
                result = varNewUInt64(cvtZToUInt64(strZ(resultStr)));
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
    jsonConsumeWhiteSpace(strZ(json), &jsonPos);

    Variant *result = jsonToNumberInternal(strZ(json), &jsonPos);

    jsonConsumeWhiteSpace(strZ(json), &jsonPos);

    if (jsonPos != strSize(json))
        THROW_FMT(JsonFormatError, "unexpected characters after number at '%s'", strZ(json) + jsonPos);

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

    String *result = strNew();

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

                (*jsonPos)++;

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

                    case 'u':
                    {
                        (*jsonPos)++;

                        // We don't know how to decode anything except ASCII so fail if it looks like Unicode
                        if (strncmp(json + *jsonPos, "00", 2) != 0)
                            THROW_FMT(JsonFormatError, "unable to decode '%.4s'", json + *jsonPos);

                        // Decode char
                        (*jsonPos) += 2;
                        strCatChr(result, (char)cvtZToUIntBase(strZ(strNewZN(json + *jsonPos, 2)), 16));
                        (*jsonPos) += 1;

                        break;
                    }

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
    jsonConsumeWhiteSpace(strZ(json), &jsonPos);

    String *result = NULL;

    if (strncmp(strZ(json), NULL_Z, 4) == 0)
        jsonPos += 4;
    else
        result = jsonToStrInternal(strZ(json), &jsonPos);

    jsonConsumeWhiteSpace(strZ(json), &jsonPos);

    if (jsonPos != strSize(json))
        THROW_FMT(JsonFormatError, "unexpected characters after string at '%s'", strZ(json) + jsonPos);

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
    jsonConsumeWhiteSpace(strZ(json), &jsonPos);

    if (strZ(json)[jsonPos] != '{')
        THROW_FMT(JsonFormatError, "expected '{' at '%s'", strZ(json) + jsonPos);

    KeyValue *result = jsonToKvInternal(strZ(json), &jsonPos);

    jsonConsumeWhiteSpace(strZ(json), &jsonPos);

    if (jsonPos != strSize(json))
        THROW_FMT(JsonFormatError, "unexpected characters after object at '%s'", strZ(json) + jsonPos);

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
    jsonConsumeWhiteSpace(strZ(json), &jsonPos);

    VariantList *result = jsonToVarLstInternal(strZ(json), &jsonPos);

    jsonConsumeWhiteSpace(strZ(json), &jsonPos);

    if (jsonPos != strSize(json))
        THROW_FMT(JsonFormatError, "unexpected characters after array at '%s'", strZ(json) + jsonPos);

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
            result = varNewStr(jsonToStrInternal(json, jsonPos));
            break;

        // Integer
        case '-':
        case '0' ... '9':
            result = jsonToNumberInternal(json, jsonPos);
            break;

        // Boolean
        case 't':
        case 'f':
            result = varNewBool(jsonToBoolInternal(json, jsonPos));
            break;

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
            result = varNewVarLst(jsonToVarLstInternal(json, jsonPos));
            break;

        // Object
        case '{':
            result = varNewKv(jsonToKvInternal(json, jsonPos));
            break;

        // Object
        default:
            THROW_FMT(JsonFormatError, "invalid type at '%s'", json + *jsonPos);
            break;
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

    const char *jsonPtr = strZ(json);
    unsigned int jsonPos = 0;

    Variant *result = jsonToVarInternal(jsonPtr, &jsonPos);

    if (jsonPos != strSize(json))
        THROW_FMT(JsonFormatError, "unexpected characters after JSON at '%s'", strZ(json) + jsonPos);

    FUNCTION_LOG_RETURN(VARIANT, result);
}

/**********************************************************************************************************************************/
const String *
jsonFromBool(const bool value)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(BOOL, value);
    FUNCTION_TEST_END();

    String *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        JsonWrite *const write = jsonWriteBool(jsonWriteNewP(), value);

        MEM_CONTEXT_PRIOR_BEGIN()
        {
            result = strNewBuf(jsonWriteResult(write));
        }
        MEM_CONTEXT_PRIOR_END();
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_TEST_RETURN(result);
}

/**********************************************************************************************************************************/
String *
jsonFromInt(int value)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(INT, value);
    FUNCTION_TEST_END();

    String *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        JsonWrite *const write = jsonWriteInt(jsonWriteNewP(), value);

        MEM_CONTEXT_PRIOR_BEGIN()
        {
            result = strNewBuf(jsonWriteResult(write));
        }
        MEM_CONTEXT_PRIOR_END();
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_TEST_RETURN(result);
}

String *
jsonFromInt64(const int64_t value)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(INT64, value);
    FUNCTION_TEST_END();

    String *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        JsonWrite *const write = jsonWriteInt64(jsonWriteNewP(), value);

        MEM_CONTEXT_PRIOR_BEGIN()
        {
            result = strNewBuf(jsonWriteResult(write));
        }
        MEM_CONTEXT_PRIOR_END();
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_TEST_RETURN(result);
}

String *
jsonFromUInt(const unsigned int value)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(INT64, value);
    FUNCTION_TEST_END();

    String *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        JsonWrite *const write = jsonWriteUInt(jsonWriteNewP(), value);

        MEM_CONTEXT_PRIOR_BEGIN()
        {
            result = strNewBuf(jsonWriteResult(write));
        }
        MEM_CONTEXT_PRIOR_END();
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_TEST_RETURN(result);
}

String *
jsonFromUInt64(const uint64_t value)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(UINT64, value);
    FUNCTION_TEST_END();

    String *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        JsonWrite *const write = jsonWriteUInt64(jsonWriteNewP(), value);

        MEM_CONTEXT_PRIOR_BEGIN()
        {
            result = strNewBuf(jsonWriteResult(write));
        }
        MEM_CONTEXT_PRIOR_END();
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_TEST_RETURN(result);
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
        strCat(json, NULL_STR);
    }
    // Else escape and output string
    else
    {
        strCatChr(json, '"');

        // Track portion of string with no escapes
        const char *stringPtr = strZ(string);
        const char *noEscape = NULL;
        size_t noEscapeSize = 0;

        for (unsigned int stringIdx = 0; stringIdx < strSize(string); stringIdx++)
        {
            switch (*stringPtr)
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

                    switch (*stringPtr)
                    {
                        case '"':
                            strCatZ(json, "\\\"");
                            break;

                        case '\\':
                            strCatZ(json, "\\\\");
                            break;

                        case '\n':
                            strCatZ(json, "\\n");
                            break;

                        case '\r':
                            strCatZ(json, "\\r");
                            break;

                        case '\t':
                            strCatZ(json, "\\t");
                            break;

                        case '\b':
                            strCatZ(json, "\\b");
                            break;

                        case '\f':
                            strCatZ(json, "\\f");
                            break;
                    }

                    break;
                }

                default:
                {
                    // If escape string is zero size then start it
                    if (noEscapeSize == 0)
                        noEscape = stringPtr;

                    noEscapeSize++;
                    break;
                }
            }

            stringPtr++;
        }

        // Copy portion of string without escapes
        if (noEscapeSize > 0)
            strCatZN(json, noEscape, noEscapeSize);

        strCatChr(json, '"');
    }

    FUNCTION_TEST_RETURN_VOID();
}

String *
jsonFromStr(const String *const value)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, value);
    FUNCTION_TEST_END();

    String *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        JsonWrite *const write = jsonWriteStr(jsonWriteNewP(), value);

        MEM_CONTEXT_PRIOR_BEGIN()
        {
            result = strNewBuf(jsonWriteResult(write));
        }
        MEM_CONTEXT_PRIOR_END();
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_TEST_RETURN(result);
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

    String *result = strCatZ(strNew(), "{");

    MEM_CONTEXT_TEMP_BEGIN()
    {
        const StringList *keyList = strLstSort(strLstNewVarLst(kvKeyList(kv)), sortOrderAsc);

        for (unsigned int keyIdx = 0; keyIdx < strLstSize(keyList); keyIdx++)
        {
            String *key = strLstGet(keyList, keyIdx);
            const Variant *value = kvGet(kv, VARSTR(key));

            // If going to add another key, prepend a comma
            if (keyIdx > 0)
                strCatZ(result, ",");

            // Keys are always strings in the output, so add starting quote and colon.
            strCatFmt(result, "\"%s\":", strZ(key));

            // NULL value
            if (value == NULL)
                strCat(result, NULL_STR);
            else
            {
                switch (varType(value))
                {
                    case varTypeKeyValue:
                        strCat(result, jsonFromKvInternal(kvDup(varKv(value))));
                        break;

                    case varTypeVariantList:
                    {
                        // If the array is empty, then do not add formatting, else process the array.
                        if (varVarLst(value) == NULL)
                            strCat(result, NULL_STR);
                        else if (varLstEmpty(varVarLst(value)))
                            strCatZ(result, "[]");
                        else
                        {
                            strCatZ(result, "[");

                            for (unsigned int arrayIdx = 0; arrayIdx < varLstSize(varVarLst(value)); arrayIdx++)
                            {
                                Variant *arrayValue = varLstGet(varVarLst(value), arrayIdx);

                                // If going to add another element, add a comma
                                if (arrayIdx > 0)
                                    strCatZ(result, ",");

                                // If array value is null
                                if (arrayValue == NULL)
                                {
                                    strCat(result, NULL_STR);
                                }
                                // If the type is a string, add leading and trailing double quotes
                                else if (varType(arrayValue) == varTypeString)
                                {
                                    jsonFromStrInternal(result, varStr(arrayValue));
                                }
                                else if (varType(arrayValue) == varTypeKeyValue)
                                {
                                    strCat(result, jsonFromKvInternal(kvDup(varKv(arrayValue))));
                                }
                                else if (varType(arrayValue) == varTypeVariantList)
                                {
                                    strCat(result, jsonFromVar(arrayValue));
                                }
                                // Numeric, Boolean or other type
                                else
                                    strCat(result, varStrForce(arrayValue));
                            }

                            strCatZ(result, "]");
                        }

                        break;
                    }

                    // String
                    case varTypeString:
                        jsonFromStrInternal(result, varStr(value));
                        break;

                    default:
                        strCat(result, varStrForce(value));
                        break;
                }
            }
        }

        result = strCatZ(result, "}");
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
        String *jsonStr = strNew();

        // If VariantList then process each item in the array. Currently the list must be KeyValue types.
        if (var == NULL)
        {
            strCat(jsonStr, NULL_STR);
        }
        else if (varType(var) == varTypeBool)
        {
            strCat(jsonStr, jsonFromBool(varBool(var)));
        }
        else if (varType(var) == varTypeUInt)
        {
            strCat(jsonStr, jsonFromUInt(varUInt(var)));
        }
        else if (varType(var) == varTypeUInt64)
        {
            strCat(jsonStr, jsonFromUInt64(varUInt64(var)));
        }
        else if (varType(var) == varTypeString)
        {
            jsonFromStrInternal(jsonStr, varStr(var));
        }
        else if (varType(var) == varTypeVariantList)
        {
            const VariantList *vl = varVarLst(var);

            // If null
            if (vl == NULL)
            {
                strCat(jsonStr, NULL_STR);
            }
            // Else if not an empty array
            else if (!varLstEmpty(vl))
            {
                strCatZ(jsonStr, "[");

                // Currently only KeyValue and String lists are supported
                for (unsigned int vlIdx = 0; vlIdx < varLstSize(vl); vlIdx++)
                {
                    // If going to add another key, append a comma
                    if (vlIdx > 0)
                        strCatZ(jsonStr, ",");

                    Variant *varSub = varLstGet(vl, vlIdx);

                    if (varSub == NULL)
                    {
                        strCat(jsonStr, NULL_STR);
                    }
                    else if (varType(varSub) == varTypeBool)
                    {
                        strCat(jsonStr, jsonFromBool(varBool(varSub)));
                    }
                    else if (varType(varSub) == varTypeKeyValue)
                    {
                        strCat(jsonStr, jsonFromKvInternal(varKv(varSub)));
                    }
                    else if (varType(varSub) == varTypeVariantList)
                    {
                        strCat(jsonStr, jsonFromVar(varSub));
                    }
                    else if (varType(varSub) == varTypeInt)
                    {
                        strCat(jsonStr, jsonFromInt(varInt(varSub)));
                    }
                    else if (varType(varSub) == varTypeInt64)
                    {
                        strCat(jsonStr, jsonFromInt64(varInt64(varSub)));
                    }
                    else if (varType(varSub) == varTypeUInt)
                    {
                        strCat(jsonStr, jsonFromUInt(varUInt(varSub)));
                    }
                    else if (varType(varSub) == varTypeUInt64)
                    {
                        strCat(jsonStr, jsonFromUInt64(varUInt64(varSub)));
                    }
                    else
                        jsonFromStrInternal(jsonStr, varStr(varSub));
                }

                // Close the array
                strCatZ(jsonStr, "]");
            }
            // Else empty array
            else
                strCatZ(jsonStr, "[]");
        }
        else if (varType(var) == varTypeKeyValue)
        {
            strCat(jsonStr, jsonFromKvInternal(varKv(var)));
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

/**********************************************************************************************************************************/
String *
jsonWriteToLog(const JsonWrite *this)
{
    return strNewFmt("{depth: %u}", this->stack != NULL ? lstSize(this->stack) : (unsigned int)(this->complete ? 1 : 0));
}
