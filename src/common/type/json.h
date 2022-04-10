/***********************************************************************************************************************************
Convert JSON to/from KeyValue
***********************************************************************************************************************************/
#ifndef COMMON_TYPE_JSON_H
#define COMMON_TYPE_JSON_H

#include "common/type/keyValue.h"

/***********************************************************************************************************************************
Minimum number of extra bytes to allocate for json that is growing or likely to grow
***********************************************************************************************************************************/
#ifndef JSON_EXTRA_MIN
    #define JSON_EXTRA_MIN                                          256
#endif

/***********************************************************************************************************************************
Object types
***********************************************************************************************************************************/
typedef struct JsonWrite JsonWrite;

#include "common/io/write.h"

/***********************************************************************************************************************************
Write Constructors
***********************************************************************************************************************************/
// !!!
typedef struct JsonWriteNewParam
{
    VAR_PARAM_HEADER;
    size_t size;
} JsonWriteNewParam;

#define jsonWriteNewP(...)                                                                                                          \
    jsonWriteNew((JsonWriteNewParam){VAR_PARAM_INIT, __VA_ARGS__})

JsonWrite *jsonWriteNew(JsonWriteNewParam param);

// !!!
JsonWrite *jsonWriteNewIo(IoWrite *write);

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
const Buffer *jsonWriteResult(JsonWrite *const this);

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

// Convert a boolean to JSON
const String *jsonFromBool(bool value);

// Convert various integer types to JSON
String *jsonFromInt(int number);
String *jsonFromInt64(int64_t number);
String *jsonFromUInt(unsigned int number);
String *jsonFromUInt64(uint64_t number);

// Convert KeyValue to JSON
String *jsonFromKv(const KeyValue *kv);

// Convert a String to JSON
String *jsonFromStr(const String *string);

// Convert Variant to JSON
String *jsonFromVar(const Variant *value);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
String *jsonWriteToLog(const JsonWrite *this);

#define FUNCTION_LOG_JSON_WRITE_TYPE                                                                                               \
    JsonWrite *
#define FUNCTION_LOG_JSON_WRITE_FORMAT(value, buffer, bufferSize)                                                                  \
    FUNCTION_LOG_STRING_OBJECT_FORMAT(value, jsonWriteToLog, buffer, bufferSize)

#endif
