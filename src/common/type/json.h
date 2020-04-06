/***********************************************************************************************************************************
Convert JSON to/from KeyValue
***********************************************************************************************************************************/
#ifndef COMMON_TYPE_JSON_H
#define COMMON_TYPE_JSON_H

#include "common/type/keyValue.h"

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
String *jsonFromVar(const Variant *var);

#endif
