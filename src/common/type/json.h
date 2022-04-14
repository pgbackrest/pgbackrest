/***********************************************************************************************************************************
Convert JSON to/from KeyValue
***********************************************************************************************************************************/
#ifndef COMMON_TYPE_JSON_H
#define COMMON_TYPE_JSON_H

#include "common/type/variant.h"

/***********************************************************************************************************************************
JSON types
***********************************************************************************************************************************/
// !!! THESE SHOULD BE STRINGID?
typedef enum
{
    jsonTypeBool,
    jsonTypeNull,
    jsonTypeNumber,
    jsonTypeString,

    jsonTypeArrayBegin,                                             // !!!
    jsonTypeArrayEnd,                                               // !!!
    jsonTypeObjectBegin,
    jsonTypeObjectEnd,
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
// !!!
JsonType jsonReadTypeNext(JsonRead *this);

// !!!
void jsonReadArrayBegin(JsonRead *this);
void jsonReadArrayEnd(JsonRead *this);

// !!!
bool jsonReadBool(JsonRead *this);

// !!!
int jsonReadInt(JsonRead *this);

// !!!
int64_t jsonReadInt64(JsonRead *this);

// !!!
String *jsonReadKey(JsonRead *this);
bool jsonReadKeyExpect(JsonRead *this, const String *key);
void jsonReadKeyRequire(JsonRead *this, const String *key);

// !!
void jsonReadNull(JsonRead *this);

// !!!
void jsonReadObjectBegin(JsonRead *this);
void jsonReadObjectEnd(JsonRead *this);

// !!!
void jsonReadSkip(JsonRead *this);

// !!!
String *jsonReadStr(JsonRead *this);

// !!!
unsigned int jsonReadUInt(JsonRead *this);

// !!!
uint64_t jsonReadUInt64(JsonRead *this);

// !!!
Variant *jsonReadVar(JsonRead *this);

/***********************************************************************************************************************************
Read Helper Functions
***********************************************************************************************************************************/
// Convert JSON to Variant
Variant *jsonToVar(const String *json);

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
// !!!
typedef struct JsonWriteNewParam
{
    VAR_PARAM_HEADER;
    String *string;
} JsonWriteNewParam;

#define jsonWriteNewP(...)                                                                                                          \
    jsonWriteNew((JsonWriteNewParam){VAR_PARAM_INIT, __VA_ARGS__})

JsonWrite *jsonWriteNew(JsonWriteNewParam param);

/***********************************************************************************************************************************
Write Functions
***********************************************************************************************************************************/
// !!!
JsonWrite *jsonWriteArrayBegin(JsonWrite *this);

// !!!
JsonWrite *jsonWriteBool(JsonWrite *this, bool value);

// !!!
JsonWrite *jsonWriteInt(JsonWrite *this, int value);
JsonWrite *jsonWriteInt64(JsonWrite *this, int64_t value);

// !!!
JsonWrite *jsonWriteJson(JsonWrite *this, const String *value);

// !!!
JsonWrite *jsonWriteKey(JsonWrite *this, const String *key);
JsonWrite *jsonWriteKeyZ(JsonWrite *this, const char *key);

// !!!
JsonWrite *jsonWriteObjectBegin(JsonWrite *this);
JsonWrite *jsonWriteObjectEnd(JsonWrite *this);

// !!!
JsonWrite *jsonWriteStr(JsonWrite *this, const String *value);

// !!!
JsonWrite *jsonWriteStrFmt(JsonWrite *this, const char *format, ...) __attribute__((format(printf, 2, 3)));

// !!!
JsonWrite *jsonWriteStrLst(JsonWrite *this, const StringList *value);

// !!!
JsonWrite *jsonWriteUInt(JsonWrite *this, unsigned int value);
JsonWrite *jsonWriteUInt64(JsonWrite *this, uint64_t value);

// !!!
JsonWrite *jsonWriteVar(JsonWrite *this, const Variant *value);

// !!!
JsonWrite *jsonWriteZ(JsonWrite *this, const char *value);

/***********************************************************************************************************************************
Write Getters/Setters
***********************************************************************************************************************************/
// !!!
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
Functions
***********************************************************************************************************************************/
__attribute__((always_inline)) static inline bool
jsonTypeContainer(const JsonType jsonType)
{
    return jsonType >= jsonTypeArrayBegin;
}

__attribute__((always_inline)) static inline bool
jsonTypeScalar(const JsonType jsonType)
{
    return jsonType < jsonTypeArrayBegin;
}

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
