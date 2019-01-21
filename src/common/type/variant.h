/***********************************************************************************************************************************
Variant Data Type

Variants are lightweight objects in that they do not have their own memory context, instead they exist in the current context in
which they are instantiated. If a variant is needed outside the current memory context, the memory context must be switched to the
old context and then back. Below is a simplified example:

    Variant *result = NULL;    <--- is created in the current memory context (referred to as "old context" below)
    MEM_CONTEXT_TEMP_BEGIN()   <--- begins a new temporary context
    {
        String *resultStr = strNewN("myNewStr"); <--- creates a string in the temporary memory context
        memContextSwitch(MEM_CONTEXT_OLD()); <--- switch to old context so creation of the variant from the string is in old context
        result = varNewUInt64(cvtZToUInt64(strPtr(resultStr))); <--- recreates variant from the string in the old context.
        memContextSwitch(MEM_CONTEXT_TEMP()); <--- switch back to the temporary context
    }
    MEM_CONTEXT_TEMP_END(); <-- frees everything created inside this temporary memory context - i.e resultStr
***********************************************************************************************************************************/
#ifndef COMMON_TYPE_VARIANT_H
#define COMMON_TYPE_VARIANT_H

#include <stdint.h>

/***********************************************************************************************************************************
Variant object
***********************************************************************************************************************************/
typedef struct Variant Variant;

/***********************************************************************************************************************************
Variant type
***********************************************************************************************************************************/
typedef enum
{
    varTypeBool,
    varTypeDouble,
    varTypeInt,
    varTypeInt64,
    varTypeKeyValue,
    varTypeString,
    varTypeVariantList,
    varTypeUInt64,
} VariantType;

#include "common/type/keyValue.h"
#include "common/type/string.h"
#include "common/type/variantList.h"

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
Variant *varNewBool(bool data);
bool varBool(const Variant *this);
bool varBoolForce(const Variant *this);

Variant *varNewDbl(double data);
double varDbl(const Variant *this);
double varDblForce(const Variant *this);

Variant *varNewInt(int data);
int varInt(const Variant *this);
int varIntForce(const Variant *this);

Variant *varNewInt64(int64_t data);
int64_t varInt64(const Variant *this);
int64_t varInt64Force(const Variant *this);

Variant *varNewKv(void);
KeyValue *varKv(const Variant *this);

Variant *varNewStr(const String *data);
Variant *varNewStrZ(const char *data);
String *varStr(const Variant *this);
String *varStrForce(const Variant *this);

Variant *varNewUInt64(uint64_t data);
uint64_t varUInt64(const Variant *this);
uint64_t varUInt64Force(const Variant *this);

Variant *varNewVarLst(const VariantList *data);
VariantList *varVarLst(const Variant *this);

Variant *varDup(const Variant *this);
bool varEq(const Variant *this1, const Variant *this2);
VariantType varType(const Variant *this);

void varFree(Variant *this);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
String *varToLog(const Variant *this);

#define FUNCTION_LOG_CONST_VARIANT_TYPE                                                                                            \
    const FUNCTION_LOG_VARIANT_TYPE
#define FUNCTION_LOG_CONST_VARIANT_FORMAT(value, buffer, bufferSize)                                                               \
    FUNCTION_LOG_VARIANT_FORMAT(value, buffer, bufferSize)

#define FUNCTION_LOG_VARIANT_TYPE                                                                                                  \
    Variant *
#define FUNCTION_LOG_VARIANT_FORMAT(value, buffer, bufferSize)                                                                     \
    FUNCTION_LOG_STRING_OBJECT_FORMAT(value, varToLog, buffer, bufferSize)

#endif
