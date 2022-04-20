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
// !!! THESE SHOULD BE STRINGID?
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
JsonRead *jsonReadNew(const String *string);

/***********************************************************************************************************************************
Read Functions
***********************************************************************************************************************************/
// Read next JSON type. This is based on an examination of the first character so there may be an error when the type is read, but
// the type will not change.
JsonType jsonReadTypeNext(JsonRead *this);

// Read array begin/end
void jsonReadArrayBegin(JsonRead *this);
void jsonReadArrayEnd(JsonRead *this);

// Read boolean
bool jsonReadBool(JsonRead *this);

// Read integer
int jsonReadInt(JsonRead *this);
int64_t jsonReadInt64(JsonRead *this);
unsigned int jsonReadUInt(JsonRead *this);
uint64_t jsonReadUInt64(JsonRead *this);

// Read key
String *jsonReadKey(JsonRead *this);

// Read an expected key
bool jsonReadKeyExpect(JsonRead *this, const String *key);
bool jsonReadKeyExpectStrId(JsonRead *this, StringId key);
bool jsonReadKeyExpectZ(JsonRead *this, const char *key);

// Read an required key
JsonRead *jsonReadKeyRequire(JsonRead *this, const String *key);
JsonRead *jsonReadKeyRequireStrId(JsonRead *this, StringId key);
JsonRead *jsonReadKeyRequireZ(JsonRead *this, const char *key);

// Read null
void jsonReadNull(JsonRead *this);

// Read object begin/end
void jsonReadObjectBegin(JsonRead *this);
void jsonReadObjectEnd(JsonRead *this);

// Skip a JSON value
void jsonReadSkip(JsonRead *this);

// Read string
String *jsonReadStr(JsonRead *this);
StringId jsonReadStrId(JsonRead *this);
StringList *jsonReadStrLst(JsonRead *this);

// Read variant
Variant *jsonReadVar(JsonRead *this);

/***********************************************************************************************************************************
Read Helper Functions
***********************************************************************************************************************************/
// Convert JSON to Variant
Variant *jsonToVar(const String *json);

// Validate JSON
void jsonValidate(const String *json);

/***********************************************************************************************************************************
Read Destructor
***********************************************************************************************************************************/
__attribute__((always_inline)) static inline void
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

JsonWrite *jsonWriteNew(JsonWriteNewParam param);

/***********************************************************************************************************************************
Write Functions
***********************************************************************************************************************************/
// Write array begin/end
JsonWrite *jsonWriteArrayBegin(JsonWrite *this);
JsonWrite *jsonWriteArrayEnd(JsonWrite *this);

// Write boolean
JsonWrite *jsonWriteBool(JsonWrite *this, bool value);

// Write integer
JsonWrite *jsonWriteInt(JsonWrite *this, int value);
JsonWrite *jsonWriteInt64(JsonWrite *this, int64_t value);
JsonWrite *jsonWriteUInt(JsonWrite *this, unsigned int value);
JsonWrite *jsonWriteUInt64(JsonWrite *this, uint64_t value);

// Write JSON
JsonWrite *jsonWriteJson(JsonWrite *this, const String *value);

// Write key
JsonWrite *jsonWriteKey(JsonWrite *this, const String *key);
JsonWrite *jsonWriteKeyStrId(JsonWrite *this, StringId key);
JsonWrite *jsonWriteKeyZ(JsonWrite *this, const char *key);

// Write null
JsonWrite *jsonWriteNull(JsonWrite *this);

// Write object begin/end
JsonWrite *jsonWriteObjectBegin(JsonWrite *this);
JsonWrite *jsonWriteObjectEnd(JsonWrite *this);

// Skip the next value
void jsonReadSkip(JsonRead *this);

// Write string
JsonWrite *jsonWriteStr(JsonWrite *this, const String *value);
JsonWrite *jsonWriteStrFmt(JsonWrite *this, const char *format, ...) __attribute__((format(printf, 2, 3)));
JsonWrite *jsonWriteStrId(JsonWrite *this, StringId value);
JsonWrite *jsonWriteStrLst(JsonWrite *this, const StringList *value);
JsonWrite *jsonWriteZ(JsonWrite *this, const char *value);

// Write variant
JsonWrite *jsonWriteVar(JsonWrite *this, const Variant *value);

/***********************************************************************************************************************************
Write Getters/Setters
***********************************************************************************************************************************/
// Get JSON result
const String *jsonWriteResult(JsonWrite *this);

/***********************************************************************************************************************************
Write Destructor
***********************************************************************************************************************************/
__attribute__((always_inline)) static inline void
jsonWriteFree(JsonWrite *const this)
{
    objFree(this);
}

/***********************************************************************************************************************************
Write Helper Functions
***********************************************************************************************************************************/
// Convert Variant to JSON
String *jsonFromVar(const Variant *value);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
String *jsonReadToLog(const JsonRead *this);

#define FUNCTION_LOG_JSON_READ_TYPE                                                                                                \
    JsonRead *
#define FUNCTION_LOG_JSON_READ_FORMAT(value, buffer, bufferSize)                                                                   \
    FUNCTION_LOG_STRING_OBJECT_FORMAT(value, jsonReadToLog, buffer, bufferSize)

String *jsonWriteToLog(const JsonWrite *this);

#define FUNCTION_LOG_JSON_WRITE_TYPE                                                                                               \
    JsonWrite *
#define FUNCTION_LOG_JSON_WRITE_FORMAT(value, buffer, bufferSize)                                                                  \
    FUNCTION_LOG_STRING_OBJECT_FORMAT(value, jsonWriteToLog, buffer, bufferSize)

#endif
