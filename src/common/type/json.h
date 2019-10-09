/***********************************************************************************************************************************
Convert JSON to/from KeyValue
***********************************************************************************************************************************/
#ifndef COMMON_TYPE_JSON_H
#define COMMON_TYPE_JSON_H

#include "common/type/keyValue.h"

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
bool jsonToBool(const String *json);
int jsonToInt(const String *json);
int64_t jsonToInt64(const String *json);
KeyValue *jsonToKv(const String *json);
String *jsonToStr(const String *json);
unsigned int jsonToUInt(const String *json);
uint64_t jsonToUInt64(const String *json);
Variant *jsonToVar(const String *json);
VariantList *jsonToVarLst(const String *json);

const String *jsonFromBool(bool value);
String *jsonFromInt(int number);
String *jsonFromInt64(int64_t number);
String *jsonFromKv(const KeyValue *kv, unsigned int indent);
String *jsonFromStr(const String *string);
String *jsonFromUInt(unsigned int number);
String *jsonFromUInt64(uint64_t number);
String *jsonFromVar(const Variant *var);

#endif
