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
Variant *varNewBool(bool data);
Variant *varNewInt(int data);
Variant *varNewInt64(int64_t data);

// Note that the KeyValue is not duplicated because it this a heavy-weight operation. It is merely moved into the same MemContext as
// the Variant.
Variant *varNewKv(KeyValue *data);

Variant *varNewStr(const String *data);
Variant *varNewStrZ(const char *data);
Variant *varNewUInt(unsigned int data);
Variant *varNewUInt64(uint64_t data);
Variant *varNewVarLst(const VariantList *data);

Variant *varDup(const Variant *this);

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

bool varBool(const Variant *this);
bool varBoolForce(const Variant *this);

int varInt(const Variant *this);
int varIntForce(const Variant *this);

int64_t varInt64(const Variant *this);
int64_t varInt64Force(const Variant *this);

KeyValue *varKv(const Variant *this);

const String *varStr(const Variant *this);
String *varStrForce(const Variant *this);

unsigned int varUInt(const Variant *this);
unsigned int varUIntForce(const Variant *this);

uint64_t varUInt64(const Variant *this);
uint64_t varUInt64Force(const Variant *this);

VariantList *varVarLst(const Variant *this);

// Variant type
__attribute__((always_inline)) static inline VariantType
varType(const Variant *const this)
{
    return THIS_PUB(Variant)->type;
}

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Test if Variants are equal
bool varEq(const Variant *this1, const Variant *this2);

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
void varFree(Variant *this);

/***********************************************************************************************************************************
Macros for constant variants

Frequently used constant variants can be declared with these macros at compile time rather than dynamically at run time.

Note that variants created in this way are declared as const so can't be modified or freed by the var*() methods.  Casting to
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

// Create a String Variant constant inline from any zero-terminated string
#define VARSTRZ(dataParam)                                                                                                         \
    ((const Variant *)&(const VariantStringPub){.type = varTypeString, .data = (String *)STR(dataParam)})

// Create a String Variant constant inline from a #define or inline string constant
#define VARSTRDEF(dataParam)                                                                                                       \
    ((const Variant *)&(const VariantStringPub){.type = varTypeString, .data = (String *)STRDEF(dataParam)})

// Create a String Variant constant inline from a String constant
#define VARSTR(dataParam)                                                                                                          \
    ((const Variant *)&(const VariantStringPub){.type = varTypeString, .data = (String *)(dataParam)})

// Used to declare String Variant constants that will be externed using VARIANT_DECLARE().  Must be used in a .c file.
#define VARIANT_STRDEF_EXTERN(name, dataParam)                                                                                     \
    const Variant *const name = VARSTRDEF(dataParam)

// Used to declare String Variant constants that will be local to the .c file.  Must be used in a .c file.
#define VARIANT_STRDEF_STATIC(name, dataParam)                                                                                     \
    static const Variant *const name = VARSTRDEF(dataParam)

// Create a UInt Variant constant inline from an unsigned int
#define VARUINT(dataParam)                                                                                                         \
    ((const Variant *)&(const VariantUIntPub){.type = varTypeUInt, .data = dataParam})

// Create a UInt64 Variant constant inline from a uint64_t
#define VARUINT64(dataParam)                                                                                                       \
    ((const Variant *)&(const VariantUInt64Pub){.type = varTypeUInt64, .data = dataParam})

// Used to extern String Variant constants declared with VARIANT_STRDEF_EXTERN/STATIC().  Must be used in a .h file.
#define VARIANT_DECLARE(name)                                                                                                      \
    extern const Variant *const name

/***********************************************************************************************************************************
Constant variants that are generally useful
***********************************************************************************************************************************/
VARIANT_DECLARE(BOOL_FALSE_VAR);
VARIANT_DECLARE(BOOL_TRUE_VAR);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
String *varToLog(const Variant *this);

#define FUNCTION_LOG_VARIANT_TYPE                                                                                                  \
    Variant *
#define FUNCTION_LOG_VARIANT_FORMAT(value, buffer, bufferSize)                                                                     \
    FUNCTION_LOG_STRING_OBJECT_FORMAT(value, varToLog, buffer, bufferSize)

#endif
