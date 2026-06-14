/***********************************************************************************************************************************
Variant Data Type

Variants are lightweight objects in that they do not have their own memory context, instead they exist in the current context in
which they are instantiated. If a variant is needed outside the current memory context, the memory context must be switched to the
old context and then back. Below is a simplified example:

    Variant *result = NULL;    <--- is created in the current memory context (referred to as "old context" below)
    MEM_CONTEXT_TEMP_BEGIN()   <--- begins a new temporary context
    {
        String *resultStr = strNewZN("myNewStr"); <--- creates a string in the temporary memory context

        MEM_CONTEXT_PRIOR_BEGIN() <--- switch to old context so creation of the variant from the string is in old context
        {
            result = varNewUInt64(cvtZToUInt64(strZ(resultStr))); <--- recreates variant from the string in the old context.
        }
        MEM_CONTEXT_PRIOR_END(); <--- switch back to the temporary context
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
    varTypeInt,
    varTypeInt64,
    varTypeKeyValue,
    varTypeString,
    varTypeUInt,
    varTypeUInt64,
    varTypeVariantList,
} VariantType;

#include "common/type/keyValue.h"
#include "common/type/string.h"
#include "common/type/variantList.h"

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
FN_EXTERN Variant *varNewBool(bool data);
FN_EXTERN Variant *varNewInt(int data);
FN_EXTERN Variant *varNewInt64(int64_t data);

// Note that the KeyValue is not duplicated because it this a heavy-weight operation. It is merely moved into the same MemContext as
// the Variant.
FN_EXTERN Variant *varNewKv(KeyValue *data);

FN_EXTERN Variant *varNewStr(const String *data);
FN_EXTERN Variant *varNewStrZ(const char *data);
FN_EXTERN Variant *varNewUInt(unsigned int data);
FN_EXTERN Variant *varNewUInt64(uint64_t data);
FN_EXTERN Variant *varNewVarLst(const VariantList *data);

FN_EXTERN Variant *varDup(const Variant *this);

/***********************************************************************************************************************************
Getters/Setters
***********************************************************************************************************************************/
#define VARIANT_COMMON                                                                                                             \
    VariantType type;                                               /* Variant type */

typedef struct VariantPub
{
    VARIANT_COMMON
} VariantPub;

typedef struct VariantBoolPub
{
    VARIANT_COMMON
    bool data;                                                      // Boolean data
} VariantBoolPub;

typedef struct VariantIntPub
{
    VARIANT_COMMON
    int data;                                                       // Signed integer data
} VariantIntPub;

typedef struct VariantInt64Pub
{
    VARIANT_COMMON
    int64_t data;                                                   // 64-bit signed integer data
} VariantInt64Pub;

typedef struct VariantStringPub
{
    VARIANT_COMMON
    String *data;                                                   // String data
} VariantStringPub;

typedef struct VariantUIntPub
{
    VARIANT_COMMON
    unsigned int data;                                              // Unsigned integer data
} VariantUIntPub;

typedef struct VariantUInt64Pub
{
    VARIANT_COMMON
    uint64_t data;                                                  // 64-bit unsigned integer data
} VariantUInt64Pub;

FN_EXTERN bool varBool(const Variant *this);
FN_EXTERN bool varBoolForce(const Variant *this);

FN_EXTERN int varInt(const Variant *this);
FN_EXTERN int varIntForce(const Variant *this);

FN_EXTERN int64_t varInt64(const Variant *this);
FN_EXTERN int64_t varInt64Force(const Variant *this);

FN_EXTERN KeyValue *varKv(const Variant *this);

FN_EXTERN const String *varStr(const Variant *this);
FN_EXTERN String *varStrForce(const Variant *this);

FN_EXTERN unsigned int varUInt(const Variant *this);
FN_EXTERN unsigned int varUIntForce(const Variant *this);

FN_EXTERN uint64_t varUInt64(const Variant *this);
FN_EXTERN uint64_t varUInt64Force(const Variant *this);

FN_EXTERN VariantList *varVarLst(const Variant *this);

// Variant type
FN_INLINE_ALWAYS VariantType
varType(const Variant *const this)
{
    return THIS_PUB(Variant)->type;
}

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Test if Variants are equal
FN_EXTERN bool varEq(const Variant *this1, const Variant *this2);

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
FN_INLINE_ALWAYS void
varFree(Variant *const this)
{
    objFree(this);
}

/***********************************************************************************************************************************
Macros for constant variants

Frequently used constant variants can be declared with these macros at compile time rather than dynamically at run time.

Note that variants created in this way are declared as const so can't be modified or freed by the var*() methods. Casting to
Variant * will generally result in a segfault.

By convention all variant constant identifiers are appended with _VAR.
***********************************************************************************************************************************/
// Create a Bool Variant constant inline from a bool
#define VARBOOL(dataParam)                                                                                                         \
    ((const Variant *)&(const VariantBoolPub){.type = varTypeBool, .data = dataParam})

// Create an Int Variant constant inline from an int
#define VARINT(dataParam)                                                                                                          \
    ((const Variant *)&(const VariantIntPub){.type = varTypeInt, .data = dataParam})

// Create an Int64 Variant constant inline from an int64_t
#define VARINT64(dataParam)                                                                                                        \
    ((const Variant *)&(const VariantInt64Pub){.type = varTypeInt64, .data = dataParam})

// Create a String Variant constant inline from any zero-terminated string. The struct must be kept in sync with VariantStringPub
// (except for const qualifiers)
typedef struct VariantStringPubConst
{
    VARIANT_COMMON
    const String *data;                                             // String data
} VariantStringPubConst;

#define VARSTRZ(dataParam)                                                                                                         \
    ((const Variant *)&(const VariantStringPubConst){.type = varTypeString, .data = (const String *)STR(dataParam)})

// Create a String Variant constant inline from a #define or inline string constant
#define VARSTRDEF(dataParam)                                                                                                       \
    ((const Variant *)&(const VariantStringPubConst){.type = varTypeString, .data = (const String *)STRDEF(dataParam)})

// Create a String Variant constant inline from a String constant
#define VARSTR(dataParam)                                                                                                          \
    ((const Variant *)&(const VariantStringPubConst){.type = varTypeString, .data = (const String *)(dataParam)})

// Used to define String Variant constants that will be externed using VARIANT_DECLARE(). Must be used in a .c file.
#define VARIANT_STRDEF_EXTERN(name, dataParam)                                                                                     \
    VR_EXTERN_DEFINE const Variant *const name = VARSTRDEF(dataParam)

// Used to define String Variant constants that will be local to the .c file. Must be used in a .c file.
#define VARIANT_STRDEF_STATIC(name, dataParam)                                                                                     \
    static const Variant *const name = VARSTRDEF(dataParam)

// Create a UInt Variant constant inline from an unsigned int
#define VARUINT(dataParam)                                                                                                         \
    ((const Variant *)&(const VariantUIntPub){.type = varTypeUInt, .data = dataParam})

// Create a UInt64 Variant constant inline from a uint64_t
#define VARUINT64(dataParam)                                                                                                       \
    ((const Variant *)&(const VariantUInt64Pub){.type = varTypeUInt64, .data = dataParam})

// Used to declare externed String Variant constants defined with VARIANT*EXTERN(). Must be used in a .h file.
#define VARIANT_DECLARE(name)                                                                                                      \
    VR_EXTERN_DECLARE const Variant *const name

/***********************************************************************************************************************************
Constant variants that are generally useful
***********************************************************************************************************************************/
VARIANT_DECLARE(BOOL_FALSE_VAR);
VARIANT_DECLARE(BOOL_TRUE_VAR);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
FN_EXTERN void varToLog(const Variant *this, StringStatic *debugLog);

#define FUNCTION_LOG_VARIANT_TYPE                                                                                                  \
    Variant *
#define FUNCTION_LOG_VARIANT_FORMAT(value, buffer, bufferSize)                                                                     \
    FUNCTION_LOG_OBJECT_FORMAT(value, varToLog, buffer, bufferSize)

#endif
