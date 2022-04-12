/***********************************************************************************************************************************
Convert JSON to/from KeyValue
***********************************************************************************************************************************/
#ifndef COMMON_TYPE_JSON_H
#define COMMON_TYPE_JSON_H

#include "common/type/variant.h"

/***********************************************************************************************************************************
JSON types
***********************************************************************************************************************************/
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
JsonType jsonReadTypeNext(JsonRead *const this);

// !!!
void jsonReadArrayBegin(JsonRead *const this);
void jsonReadArrayEnd(JsonRead *const this);

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
const String *jsonWriteResult(JsonWrite *const this);

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

// Convert bool to JSON
__attribute__((always_inline)) static inline String *
jsonFromBool(const bool value)
{
    return jsonFromVar(VARBOOL(value));
}

// Convert integers to JSON
__attribute__((always_inline)) static inline String *
jsonFromInt(const int value)
{
    return jsonFromVar(VARINT(value));
}

__attribute__((always_inline)) static inline String *
jsonFromInt64(const int64_t value)
{
    return jsonFromVar(VARINT64(value));
}

__attribute__((always_inline)) static inline String *
jsonFromUInt(const unsigned int value)
{
    return jsonFromVar(VARUINT(value));
}

__attribute__((always_inline)) static inline String *
jsonFromUInt64(const uint64_t value)
{
    return jsonFromVar(VARUINT64(value));
}

// Convert String to JSON
__attribute__((always_inline)) static inline String *
jsonFromStr(const String *const value)
{
    return jsonFromVar(VARSTR(value));
}

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
Functions
***********************************************************************************************************************************/
// Convert a json string to a bool
bool jsonToBool(const String *json);

// Convert a json number to various integer types
int jsonToInt(const String *json);
int64_t jsonToInt64(const String *json);
unsigned int jsonToUInt(const String *json);
uint64_t jsonToUInt64(const String *json);

// Convert a json object to a KeyValue
KeyValue *jsonToKv(const String *json);

// Convert a json string to a String
String *jsonToStr(const String *json);

// Convert JSON to a variant
Variant *jsonToVar(const String *json);

// Convert a json array to a VariantList
VariantList *jsonToVarLst(const String *json);

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
