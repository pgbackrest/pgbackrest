/***********************************************************************************************************************************
JSON read/write
***********************************************************************************************************************************/
#ifndef COMMON_TYPE_JSON_H
#define COMMON_TYPE_JSON_H

#include "common/type/stringId.h"
#include "common/type/variant.h"

/***********************************************************************************************************************************
JSON types
***********************************************************************************************************************************/
typedef enum
{
    jsonTypeBool = STRID5("bool", 0x63de20),                        // Boolean
    jsonTypeNull = STRID5("null", 0x632ae0),                        // Null
    jsonTypeNumber = STRID5("nmbr", 0x909ae0),                      // Number
    jsonTypeString = STRID5("str", 0x4a930),                        // String

    jsonTypeArrayBegin = STRID5("ary-b", 0x2de6410),                // Array begin
    jsonTypeArrayEnd = STRID5("ary-e", 0x5de6410),                  // Array end
    jsonTypeObjectBegin = STRID5("obj-b", 0x2da84f0),               // Object begin
    jsonTypeObjectEnd = STRID5("obj-e", 0x5da84f0),                 // Object end
} JsonType;

/***********************************************************************************************************************************
Object types
***********************************************************************************************************************************/
typedef struct JsonRead JsonRead;
typedef struct JsonWrite JsonWrite;

/***********************************************************************************************************************************
Read Constructors
***********************************************************************************************************************************/
FV_EXTERN JsonRead *jsonReadNew(const String *string);

/***********************************************************************************************************************************
Read Functions
***********************************************************************************************************************************/
// Read next JSON type. This is based on an examination of the first character so there may be an error when the type is read, but
// the type will not change.
FV_EXTERN JsonType jsonReadTypeNext(JsonRead *this);

// Read array begin/end
FV_EXTERN void jsonReadArrayBegin(JsonRead *this);
FV_EXTERN void jsonReadArrayEnd(JsonRead *this);

// Read boolean
FV_EXTERN bool jsonReadBool(JsonRead *this);

// Read integer
FV_EXTERN int jsonReadInt(JsonRead *this);
FV_EXTERN int64_t jsonReadInt64(JsonRead *this);
FV_EXTERN unsigned int jsonReadUInt(JsonRead *this);
FV_EXTERN uint64_t jsonReadUInt64(JsonRead *this);

// Read key
FV_EXTERN String *jsonReadKey(JsonRead *this);

// Read an expected key
FV_EXTERN bool jsonReadKeyExpect(JsonRead *this, const String *key);
FV_EXTERN bool jsonReadKeyExpectStrId(JsonRead *this, StringId key);
FV_EXTERN bool jsonReadKeyExpectZ(JsonRead *this, const char *key);

// Read an required key
FV_EXTERN JsonRead *jsonReadKeyRequire(JsonRead *this, const String *key);
FV_EXTERN JsonRead *jsonReadKeyRequireStrId(JsonRead *this, StringId key);
FV_EXTERN JsonRead *jsonReadKeyRequireZ(JsonRead *this, const char *key);

// Read null
FV_EXTERN void jsonReadNull(JsonRead *this);

// Read object begin/end
FV_EXTERN void jsonReadObjectBegin(JsonRead *this);
FV_EXTERN void jsonReadObjectEnd(JsonRead *this);

// Skip value
FV_EXTERN void jsonReadSkip(JsonRead *this);

// Read string
FV_EXTERN String *jsonReadStr(JsonRead *this);
FV_EXTERN StringId jsonReadStrId(JsonRead *this);
FV_EXTERN StringList *jsonReadStrLst(JsonRead *this);

// Read variant
FV_EXTERN Variant *jsonReadVar(JsonRead *this);

/***********************************************************************************************************************************
Read Helper Functions
***********************************************************************************************************************************/
// Convert JSON to Variant
FV_EXTERN Variant *jsonToVar(const String *json);

// Validate JSON
FV_EXTERN void jsonValidate(const String *json);

/***********************************************************************************************************************************
Read Destructor
***********************************************************************************************************************************/
FN_INLINE_ALWAYS void
jsonReadFree(JsonRead *const this)
{
    objFree(this);
}

/***********************************************************************************************************************************
Write Constructors
***********************************************************************************************************************************/
typedef struct JsonWriteNewParam
{
    VAR_PARAM_HEADER;
    String *json;
} JsonWriteNewParam;

#define jsonWriteNewP(...)                                                                                                          \
    jsonWriteNew((JsonWriteNewParam){VAR_PARAM_INIT, __VA_ARGS__})

FV_EXTERN JsonWrite *jsonWriteNew(JsonWriteNewParam param);

/***********************************************************************************************************************************
Write Functions
***********************************************************************************************************************************/
// Write array begin/end
FV_EXTERN JsonWrite *jsonWriteArrayBegin(JsonWrite *this);
FV_EXTERN JsonWrite *jsonWriteArrayEnd(JsonWrite *this);

// Write boolean
FV_EXTERN JsonWrite *jsonWriteBool(JsonWrite *this, bool value);

// Write integer
FV_EXTERN JsonWrite *jsonWriteInt(JsonWrite *this, int value);
FV_EXTERN JsonWrite *jsonWriteInt64(JsonWrite *this, int64_t value);
FV_EXTERN JsonWrite *jsonWriteUInt(JsonWrite *this, unsigned int value);
FV_EXTERN JsonWrite *jsonWriteUInt64(JsonWrite *this, uint64_t value);

// Write JSON
FV_EXTERN JsonWrite *jsonWriteJson(JsonWrite *this, const String *value);

// Write key
FV_EXTERN JsonWrite *jsonWriteKey(JsonWrite *this, const String *key);
FV_EXTERN JsonWrite *jsonWriteKeyStrId(JsonWrite *this, StringId key);
FV_EXTERN JsonWrite *jsonWriteKeyZ(JsonWrite *this, const char *key);

// Write null
FV_EXTERN JsonWrite *jsonWriteNull(JsonWrite *this);

// Write object begin/end
FV_EXTERN JsonWrite *jsonWriteObjectBegin(JsonWrite *this);
FV_EXTERN JsonWrite *jsonWriteObjectEnd(JsonWrite *this);

// Write string
FV_EXTERN JsonWrite *jsonWriteStr(JsonWrite *this, const String *value);
FV_EXTERN JsonWrite *jsonWriteStrFmt(JsonWrite *this, const char *format, ...) __attribute__((format(printf, 2, 3)));
FV_EXTERN JsonWrite *jsonWriteStrId(JsonWrite *this, StringId value);
FV_EXTERN JsonWrite *jsonWriteStrLst(JsonWrite *this, const StringList *value);
FV_EXTERN JsonWrite *jsonWriteZ(JsonWrite *this, const char *value);

// Write variant
FV_EXTERN JsonWrite *jsonWriteVar(JsonWrite *this, const Variant *value);

/***********************************************************************************************************************************
Write Getters/Setters
***********************************************************************************************************************************/
// Get JSON result
FV_EXTERN const String *jsonWriteResult(JsonWrite *this);

/***********************************************************************************************************************************
Write Destructor
***********************************************************************************************************************************/
FN_INLINE_ALWAYS void
jsonWriteFree(JsonWrite *const this)
{
    objFree(this);
}

/***********************************************************************************************************************************
Write Helper Functions
***********************************************************************************************************************************/
// Convert Variant to JSON
FV_EXTERN String *jsonFromVar(const Variant *value);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#ifdef DEBUG

FV_EXTERN String *jsonReadToLog(const JsonRead *this);

#define FUNCTION_LOG_JSON_READ_TYPE                                                                                                \
    JsonRead *
#define FUNCTION_LOG_JSON_READ_FORMAT(value, buffer, bufferSize)                                                                   \
    FUNCTION_LOG_STRING_OBJECT_FORMAT(value, jsonReadToLog, buffer, bufferSize)

FV_EXTERN String *jsonWriteToLog(const JsonWrite *this);

#define FUNCTION_LOG_JSON_WRITE_TYPE                                                                                               \
    JsonWrite *
#define FUNCTION_LOG_JSON_WRITE_FORMAT(value, buffer, bufferSize)                                                                  \
    FUNCTION_LOG_STRING_OBJECT_FORMAT(value, jsonWriteToLog, buffer, bufferSize)

#endif // DEBUG

#endif
