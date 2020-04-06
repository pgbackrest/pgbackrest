/***********************************************************************************************************************************
Variant Data Type
***********************************************************************************************************************************/
#include "build.auto.h"

#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "common/debug.h"
#include "common/memContext.h"
#include "common/type/convert.h"
#include "common/type/variant.h"

/***********************************************************************************************************************************
Constant variants that are generally useful
***********************************************************************************************************************************/
// Used to declare Bool Variant constants that will be externed using VARIANT_DECLARE().  Must be used in a .c file.
#define VARIANT_BOOL_EXTERN(name, dataParam)                                                                                       \
    const Variant *const name = ((const Variant *)&(const VariantBoolConst){.type = varTypeBool, .data = dataParam})

VARIANT_BOOL_EXTERN(BOOL_FALSE_VAR,                                 false);
VARIANT_BOOL_EXTERN(BOOL_TRUE_VAR,                                  true);

/***********************************************************************************************************************************
Information about the variant
***********************************************************************************************************************************/
struct Variant
{
    VARIANT_COMMON
};

typedef struct VariantBool
{
    VARIANT_COMMON
    VARIANT_BOOL_COMMON
    MemContext *memContext;
} VariantBool;

typedef struct VariantDouble
{
    VARIANT_COMMON
    VARIANT_DOUBLE_COMMON
    MemContext *memContext;
} VariantDouble;

typedef struct VariantInt
{
    VARIANT_COMMON
    VARIANT_INT_COMMON
    MemContext *memContext;
} VariantInt;

typedef struct VariantInt64
{
    VARIANT_COMMON
    VARIANT_INT64_COMMON
    MemContext *memContext;
} VariantInt64;

typedef struct VariantKeyValue
{
    VARIANT_COMMON
    KeyValue *data;                                                 /* KeyValue data */
    MemContext *memContext;
} VariantKeyValue;

typedef struct VariantString
{
    VARIANT_COMMON
    VARIANT_STRING_COMMON
    MemContext *memContext;
} VariantString;

typedef struct VariantUInt
{
    VARIANT_COMMON
    VARIANT_UINT_COMMON
    MemContext *memContext;
} VariantUInt;

typedef struct VariantUInt64
{
    VARIANT_COMMON
    VARIANT_UINT64_COMMON
    MemContext *memContext;
} VariantUInt64;

typedef struct VariantVariantList
{
    VARIANT_COMMON
    VariantList *data;                                              /* VariantList data */
    MemContext *memContext;
} VariantVariantList;

/***********************************************************************************************************************************
Variant type names
***********************************************************************************************************************************/
static const char *const variantTypeName[] =
{
    "bool",                                                         // varTypeBool
    "double",                                                       // varTypeDouble,
    "int",                                                          // varTypeInt
    "int64",                                                        // varTypeInt64
    "KeyValue",                                                     // varTypeKeyValue
    "String",                                                       // varTypeString
    "unsigned int",                                                 // varTypeUInt
    "uint64",                                                       // varTypeUInt64
    "VariantList",                                                  // varTypeVariantList
};

/**********************************************************************************************************************************/
Variant *
varDup(const Variant *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(VARIANT, this);
    FUNCTION_TEST_END();

    Variant *result = NULL;

    if (this != NULL)
    {
        switch (this->type)
        {
            case varTypeBool:
            {
                result = varNewBool(varBool(this));
                break;
            }

            case varTypeDouble:
            {
                result = varNewDbl(varDbl(this));
                break;
            }

            case varTypeInt:
            {
                result = varNewInt(varInt(this));
                break;
            }

            case varTypeInt64:
            {
                result = varNewInt64(varInt64(this));
                break;
            }

            case varTypeKeyValue:
            {
                VariantKeyValue *keyValue = memNew(sizeof(VariantKeyValue));

                *keyValue = (VariantKeyValue)
                {
                    .memContext = memContextCurrent(),
                    .type = varTypeKeyValue,
                    .data = kvDup(varKv(this)),
                };

                result = (Variant *)keyValue;
                break;
            }

            case varTypeString:
            {
                result = varNewStr(varStr(this));
                break;
            }

            case varTypeUInt:
            {
                result = varNewUInt(varUInt(this));
                break;
            }

            case varTypeUInt64:
            {
                result = varNewUInt64(varUInt64(this));
                break;
            }

            case varTypeVariantList:
            {
                result = varNewVarLst(varVarLst(this));
                break;
            }
        }
    }

    FUNCTION_TEST_RETURN(result);
}

/**********************************************************************************************************************************/
bool
varEq(const Variant *this1, const Variant *this2)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(VARIANT, this1);
        FUNCTION_TEST_PARAM(VARIANT, this2);
    FUNCTION_TEST_END();

    bool result = false;

    // Test if both variants are non-null
    if (this1 != NULL && this2 != NULL)
    {
        // Test if both variants are of the same type
        if (varType(this1) == varType(this2))
        {
            switch (varType(this1))
            {
                case varTypeBool:
                {
                    result = varBool(this1) == varBool(this2);
                    break;
                }

                case varTypeDouble:
                {
                    result = varDbl(this1) == varDbl(this2);
                    break;
                }

                case varTypeInt:
                {
                    result = varInt(this1) == varInt(this2);
                    break;
                }

                case varTypeInt64:
                {
                    result = varInt64(this1) == varInt64(this2);
                    break;
                }

                case varTypeString:
                {
                    result = strEq(varStr(this1), varStr(this2));
                    break;
                }

                case varTypeUInt:
                {
                    result = varUInt(this1) == varUInt(this2);
                    break;
                }

                case varTypeUInt64:
                {
                    result = varUInt64(this1) == varUInt64(this2);
                    break;
                }

                default:
                    THROW_FMT(AssertError, "unable to test equality for %s", variantTypeName[this1->type]);
            }
        }
    }
    // Else they are equal if they are both null
    else
        result = this1 == NULL && this2 == NULL;

    FUNCTION_TEST_RETURN(result);
}

/**********************************************************************************************************************************/
VariantType
varType(const Variant *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(VARIANT, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->type);
}

/**********************************************************************************************************************************/
Variant *
varNewBool(bool data)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(BOOL, data);
    FUNCTION_TEST_END();

    // Allocate memory for the variant and set the type and data
    VariantBool *this = memNew(sizeof(VariantBool));

    *this = (VariantBool)
    {
        .memContext = memContextCurrent(),
        .type = varTypeBool,
        .data = data,
    };

    FUNCTION_TEST_RETURN((Variant *)this);
}

/**********************************************************************************************************************************/
bool
varBool(const Variant *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(VARIANT, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(this->type == varTypeBool);

    FUNCTION_TEST_RETURN(((VariantBool *)this)->data);
}

bool
varBoolForce(const Variant *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(VARIANT, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    bool result = false;

    switch (this->type)
    {
        case varTypeBool:
            result = varBool(this);
            break;

        case varTypeInt:
            result = varInt(this) != 0;
            break;

        case varTypeInt64:
            result = varInt64(this) != 0;
            break;

        case varTypeString:
        {
            // List of false/true boolean string values.  Note that false/true values must be equal.
            static const char *const boolString[] =
            {
                "n", "f", "0",  "no", FALSE_Z, "off",
                "y", "t", "1", "yes",  TRUE_Z,  "on",
            };

            // Search for the string
            const char *string = strPtr(varStr(this));
            unsigned int boolIdx;

            for (boolIdx = 0; boolIdx < sizeof(boolString) / sizeof(char *); boolIdx++)
                if (strcasecmp(string, boolString[boolIdx]) == 0)
                    break;

            // If string was not found then not a boolean
            if (boolIdx == sizeof(boolString) / sizeof(char *))
                THROW_FMT(FormatError, "unable to convert str '%s' to bool", string);

            // False if in first half of list, true if in second half
            result = boolIdx / (sizeof(boolString) / sizeof(char *) / 2);

            break;
        }

        case varTypeUInt:
            result = varUInt(this) != 0;
            break;

        case varTypeUInt64:
            result = varUInt64(this) != 0;
            break;

        default:
            THROW_FMT(AssertError, "unable to force %s to %s", variantTypeName[this->type], variantTypeName[varTypeBool]);
    }

    FUNCTION_TEST_RETURN(result);
}

/**********************************************************************************************************************************/
Variant *
varNewDbl(double data)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(DOUBLE, data);
    FUNCTION_TEST_END();

    // Allocate memory for the variant and set the type and data
    VariantDouble *this = memNew(sizeof(VariantDouble));

    *this = (VariantDouble)
    {
        .memContext = memContextCurrent(),
        .type = varTypeDouble,
        .data = data,
    };

    FUNCTION_TEST_RETURN((Variant *)this);
}

/**********************************************************************************************************************************/
double
varDbl(const Variant *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(VARIANT, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(this->type == varTypeDouble);

    FUNCTION_TEST_RETURN(((VariantDouble *)this)->data);
}

double
varDblForce(const Variant *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(VARIANT, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    double result = 0;

    switch (this->type)
    {
        case varTypeBool:
        {
            result = varBool(this);
            break;
        }

        case varTypeDouble:
        {
            result = varDbl(this);
            break;
        }

        case varTypeInt:
        {
            result = varInt(this);
            break;
        }

        case varTypeInt64:
        {
            result = (double)varInt64(this);
            break;
        }

        case varTypeString:
        {
            result = cvtZToDouble(strPtr(varStr(this)));
            break;
        }

        case varTypeUInt:
        {
            result = (double)varUInt(this);
            break;
        }

        case varTypeUInt64:
        {
            result = (double)varUInt64(this);
            break;
        }

        default:
            THROW_FMT(AssertError, "unable to force %s to %s", variantTypeName[this->type], variantTypeName[varTypeDouble]);
    }

    FUNCTION_TEST_RETURN(result);
}

/**********************************************************************************************************************************/
Variant *
varNewInt(int data)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(INT, data);
    FUNCTION_TEST_END();

    // Allocate memory for the variant and set the type and data
    VariantInt *this = memNew(sizeof(VariantInt));

    *this = (VariantInt)
    {
        .memContext = memContextCurrent(),
        .type = varTypeInt,
        .data = data,
    };

    FUNCTION_TEST_RETURN((Variant *)this);
}

/**********************************************************************************************************************************/
int
varInt(const Variant *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(VARIANT, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(this->type == varTypeInt);

    FUNCTION_TEST_RETURN(((VariantInt *)this)->data);
}

int
varIntForce(const Variant *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(VARIANT, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    int result = 0;

    switch (this->type)
    {
        case varTypeBool:
        {
            result = varBool(this);
            break;
        }

        case varTypeInt:
        {
            result = varInt(this);
            break;
        }

        case varTypeInt64:
        {
            int64_t resultTest = varInt64(this);

            // Make sure the value fits into a normal 32-bit int range since 32-bit platforms are supported
            if (resultTest > INT32_MAX || resultTest < INT32_MIN)
                THROW_FMT(
                    FormatError, "unable to convert %s %" PRId64 " to %s", variantTypeName[this->type], resultTest,
                    variantTypeName[varTypeInt]);

            result = (int)resultTest;
            break;
        }

        case varTypeString:
        {
            result = cvtZToInt(strPtr(varStr(this)));
            break;
        }

        case varTypeUInt:
        {
            unsigned int resultTest = varUInt(this);

            // Make sure the value fits into a normal 32-bit int range
            if (resultTest > INT32_MAX)
                THROW_FMT(
                    FormatError, "unable to convert %s %u to %s", variantTypeName[this->type], resultTest,
                    variantTypeName[varTypeInt]);

            result = (int)resultTest;
            break;
        }

        case varTypeUInt64:
        {
            uint64_t resultTest = varUInt64(this);

            // Make sure the value fits into a normal 32-bit int range
            if (resultTest > INT32_MAX)
                THROW_FMT(
                    FormatError, "unable to convert %s %" PRIu64 " to %s", variantTypeName[this->type], resultTest,
                    variantTypeName[varTypeInt]);

            result = (int)resultTest;
            break;
        }

        default:
            THROW_FMT(AssertError, "unable to force %s to %s", variantTypeName[this->type], variantTypeName[varTypeInt]);
    }

    FUNCTION_TEST_RETURN(result);
}

/**********************************************************************************************************************************/
Variant *
varNewInt64(int64_t data)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(INT64, data);
    FUNCTION_TEST_END();

    // Allocate memory for the variant and set the type and data
    VariantInt64 *this = memNew(sizeof(VariantInt64));

    *this = (VariantInt64)
    {
        .memContext = memContextCurrent(),
        .type = varTypeInt64,
        .data = data,
    };

    FUNCTION_TEST_RETURN((Variant *)this);
}

/**********************************************************************************************************************************/
int64_t
varInt64(const Variant *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(VARIANT, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(this->type == varTypeInt64);

    FUNCTION_TEST_RETURN(((VariantInt64 *)this)->data);
}

int64_t
varInt64Force(const Variant *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(VARIANT, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    int64_t result = 0;

    switch (this->type)
    {
        case varTypeBool:
        {
            result = varBool(this);
            break;
        }

        case varTypeInt:
        {
            result = (int64_t)varInt(this);
            break;
        }

        case varTypeInt64:
        {
            result = varInt64(this);
            break;
        }

        case varTypeString:
        {
            result = cvtZToInt64(strPtr(varStr(this)));
            break;
        }

        case varTypeUInt:
        {
            result = varUInt(this);

            break;
        }

        case varTypeUInt64:
        {
            uint64_t resultTest = varUInt64(this);

            // If max number of unsigned 64-bit integer is greater than max 64-bit signed integer can hold, then error
            if (resultTest <= INT64_MAX)
                result = (int64_t)resultTest;
            else
            {
                THROW_FMT(
                    FormatError, "unable to convert %s %" PRIu64 " to %s", variantTypeName[this->type], resultTest,
                    variantTypeName[varTypeInt64]);
            }

            break;
        }

        default:
            THROW_FMT(AssertError, "unable to force %s to %s", variantTypeName[this->type], variantTypeName[varTypeInt64]);
    }

    FUNCTION_TEST_RETURN(result);
}

/**********************************************************************************************************************************/
Variant *
varNewUInt(unsigned int data)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(UINT, data);
    FUNCTION_TEST_END();

    // Allocate memory for the variant and set the type and data
    VariantUInt *this = memNew(sizeof(VariantUInt));

    *this = (VariantUInt)
    {
        .memContext = memContextCurrent(),
        .type = varTypeUInt,
        .data = data,
    };

    FUNCTION_TEST_RETURN((Variant *)this);
}

/**********************************************************************************************************************************/
unsigned int
varUInt(const Variant *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(VARIANT, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(this->type == varTypeUInt);

    FUNCTION_TEST_RETURN(((VariantUInt *)this)->data);
}

unsigned int
varUIntForce(const Variant *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(VARIANT, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    unsigned int result = 0;

    switch (this->type)
    {
        case varTypeBool:
        {
            result = varBool(this);
            break;
        }

        case varTypeInt:
        {
            int resultTest = varInt(this);

            // If integer is a negative number, throw an error since the resulting conversion would be a different number
            if (resultTest >= 0)
                result = (unsigned int)resultTest;
            else
            {
                THROW_FMT(
                    FormatError, "unable to convert %s %d to %s", variantTypeName[this->type], resultTest,
                    variantTypeName[varTypeUInt]);
            }

            break;
        }

        case varTypeInt64:
        {
            int64_t resultTest = varInt64(this);

            // If integer is a negative number or too large, throw an error since the resulting conversion would be out of bounds
            if (resultTest >= 0 && resultTest <= UINT_MAX)
                result = (unsigned int)resultTest;
            else
            {
                THROW_FMT(
                    FormatError, "unable to convert %s %" PRId64 " to %s", variantTypeName[this->type], resultTest,
                    variantTypeName[varTypeUInt]);
            }

            break;
        }

        case varTypeUInt:
        {
            result = varUInt(this);
            break;
        }

        case varTypeUInt64:
        {
            uint64_t resultTest = varUInt64(this);

            // If integer is too large, throw an error since the resulting conversion would be out of bounds
            if (resultTest <= UINT_MAX)
                result = (unsigned int)resultTest;
            else
            {
                THROW_FMT(
                    FormatError, "unable to convert %s %" PRIu64 " to %s", variantTypeName[this->type], resultTest,
                    variantTypeName[varTypeUInt]);
            }

            break;
        }

        case varTypeString:
        {
            result = cvtZToUInt(strPtr(varStr(this)));
            break;
        }

        default:
            THROW_FMT(AssertError, "unable to force %s to %s", variantTypeName[this->type], variantTypeName[varTypeUInt]);
    }

    FUNCTION_TEST_RETURN(result);
}

/**********************************************************************************************************************************/
Variant *
varNewUInt64(uint64_t data)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(UINT64, data);
    FUNCTION_TEST_END();

    // Allocate memory for the variant and set the type and data
    VariantUInt64 *this = memNew(sizeof(VariantUInt64));

    *this = (VariantUInt64)
    {
        .memContext = memContextCurrent(),
        .type = varTypeUInt64,
        .data = data,
    };

    FUNCTION_TEST_RETURN((Variant *)this);
}

/**********************************************************************************************************************************/
uint64_t
varUInt64(const Variant *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(VARIANT, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(this->type == varTypeUInt64);

    FUNCTION_TEST_RETURN(((VariantUInt64 *)this)->data);
}

uint64_t
varUInt64Force(const Variant *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(VARIANT, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    uint64_t result = 0;

    switch (this->type)
    {
        case varTypeBool:
        {
            result = varBool(this);
            break;
        }

        case varTypeInt:
        {
            int resultTest = varInt(this);

            // If integer is a negative number, throw an error since the resulting conversion would be a different number
            if (resultTest >= 0)
                result = (uint64_t)resultTest;
            else
            {
                THROW_FMT(
                    FormatError, "unable to convert %s %d to %s", variantTypeName[this->type], resultTest,
                    variantTypeName[varTypeUInt64]);
            }

            break;
        }

        case varTypeInt64:
        {
            int64_t resultTest = varInt64(this);

            // If integer is a negative number, throw an error since the resulting conversion would be out of bounds
            if (resultTest >= 0)
                result = (uint64_t)resultTest;
            else
            {
                THROW_FMT(
                    FormatError, "unable to convert %s %" PRId64 " to %s", variantTypeName[this->type], resultTest,
                    variantTypeName[varTypeUInt64]);
            }

            break;
        }

        case varTypeString:
        {
            result = cvtZToUInt64(strPtr(varStr(this)));
            break;
        }

        case varTypeUInt:
        {
            result = varUInt(this);
            break;
        }

        case varTypeUInt64:
        {
            result = varUInt64(this);
            break;
        }

        default:
            THROW_FMT(AssertError, "unable to force %s to %s", variantTypeName[this->type], variantTypeName[varTypeUInt64]);
    }

    FUNCTION_TEST_RETURN(result);
}

/**********************************************************************************************************************************/
Variant *
varNewKv(KeyValue *data)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(KEY_VALUE, data);
    FUNCTION_TEST_END();

    // Allocate memory for the variant and set the type and data
    VariantKeyValue *this = memNew(sizeof(VariantKeyValue));

    *this = (VariantKeyValue)
    {
        .memContext = memContextCurrent(),
        .type = varTypeKeyValue,
    };

    if (data != NULL)
        this->data = kvMove(data, memContextCurrent());

    FUNCTION_TEST_RETURN((Variant *)this);
}

/**********************************************************************************************************************************/
KeyValue *
varKv(const Variant *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(VARIANT, this);
    FUNCTION_TEST_END();

    KeyValue *result = NULL;

    if (this != NULL)
    {
        ASSERT(this->type == varTypeKeyValue);
        result = ((VariantKeyValue *)this)->data;
    }

    FUNCTION_TEST_RETURN(result);
}

/**********************************************************************************************************************************/
Variant *
varNewStr(const String *data)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, data);
    FUNCTION_TEST_END();

    // Allocate memory for the variant and set the type and data
    VariantString *this = memNew(sizeof(VariantString));

    *this = (VariantString)
    {
        .memContext = memContextCurrent(),
        .type = varTypeString,
        .data = strDup(data),
    };

    FUNCTION_TEST_RETURN((Variant *)this);
}

Variant *
varNewStrZ(const char *data)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRINGZ, data);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RETURN(varNewStr(data == NULL ? NULL : strNew(data)));
}

/**********************************************************************************************************************************/
const String *
varStr(const Variant *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(VARIANT, this);
    FUNCTION_TEST_END();

    String *result = NULL;

    if (this != NULL)
    {
        ASSERT(this->type == varTypeString);
        result = ((VariantString *)this)->data;
    }

    FUNCTION_TEST_RETURN(result);
}

String *
varStrForce(const Variant *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(VARIANT, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    String *result = NULL;

    switch (varType(this))
    {
        case varTypeBool:
        {
            result = strNew(cvtBoolToConstZ(varBool(this)));
            break;
        }

        case varTypeDouble:
        {
            char working[CVT_BASE10_BUFFER_SIZE];

            cvtDoubleToZ(varDbl(this), working, sizeof(working));
            result = strNew(working);
            break;
        }

        case varTypeInt:
        {
            char working[CVT_BASE10_BUFFER_SIZE];

            cvtIntToZ(varInt(this), working, sizeof(working));
            result = strNew(working);
            break;
        }

        case varTypeInt64:
        {
            char working[CVT_BASE10_BUFFER_SIZE];

            cvtInt64ToZ(varInt64(this), working, sizeof(working));
            result = strNew(working);
            break;
        }

        case varTypeString:
        {
            result = strDup(varStr(this));
            break;
        }

        case varTypeUInt:
        {
            char working[CVT_BASE10_BUFFER_SIZE];

            cvtUIntToZ(varUInt(this), working, sizeof(working));
            result = strNew(working);
            break;
        }

        case varTypeUInt64:
        {
            char working[CVT_BASE10_BUFFER_SIZE];

            cvtUInt64ToZ(varUInt64(this), working, sizeof(working));
            result = strNew(working);
            break;
        }

        default:
            THROW_FMT(FormatError, "unable to force %s to %s", variantTypeName[this->type], variantTypeName[varTypeString]);
    }

    FUNCTION_TEST_RETURN(result);
}

/**********************************************************************************************************************************/
Variant *
varNewVarLst(const VariantList *data)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(VARIANT_LIST, data);
    FUNCTION_TEST_END();

    // Allocate memory for the variant and set the type and data
    VariantVariantList *this = memNew(sizeof(VariantVariantList));

    *this = (VariantVariantList)
    {
        .memContext = memContextCurrent(),
        .type = varTypeVariantList,
    };

    if (data != NULL)
        this->data = varLstDup(data);

    FUNCTION_TEST_RETURN((Variant *)this);
}

/**********************************************************************************************************************************/
VariantList *
varVarLst(const Variant *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(VARIANT, this);
    FUNCTION_TEST_END();

    VariantList *result = NULL;

    if (this != NULL)
    {
        ASSERT(this->type == varTypeVariantList);
        result = ((VariantVariantList *)this)->data;
    }

    FUNCTION_TEST_RETURN(result);
}

/**********************************************************************************************************************************/
String *
varToLog(const Variant *this)
{
    String *result = NULL;

    if (this == NULL)
        result = strDup(NULL_STR);
    else
    {
        switch (varType(this))
        {
            case varTypeString:
            {
                result = strToLog(varStr(this));
                break;
            }

            case varTypeKeyValue:
            {
                result = strNew("{KeyValue}");
                break;
            }

            case varTypeVariantList:
            {
                result = strNew("{VariantList}");
                break;
            }

            case varTypeBool:
            case varTypeDouble:
            case varTypeInt:
            case varTypeInt64:
            case varTypeUInt:
            case varTypeUInt64:
            {
                result = strNewFmt("{%s}", strPtr(varStrForce(this)));
                break;
            }
        }
    }

    return result;
}

/**********************************************************************************************************************************/
void
varFree(Variant *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(VARIANT, this);
    FUNCTION_TEST_END();

    if (this != NULL)
    {
        MemContext *memContext = NULL;

        switch (this->type)
        {
            case varTypeBool:
            {
                memContext = ((VariantBool *)this)->memContext;
                break;
            }

            case varTypeDouble:
            {
                memContext = ((VariantDouble *)this)->memContext;
                break;
            }

            case varTypeInt:
            {
                memContext = ((VariantInt *)this)->memContext;
                break;
            }

            case varTypeInt64:
            {
                memContext = ((VariantInt64 *)this)->memContext;
                break;
            }

            case varTypeKeyValue:
            {
                memContext = ((VariantKeyValue *)this)->memContext;
                kvFree(((VariantKeyValue *)this)->data);
                break;
            }

            case varTypeString:
            {
                memContext = ((VariantString *)this)->memContext;
                strFree(((VariantString *)this)->data);
                break;
            }

            case varTypeUInt:
            {
                memContext = ((VariantUInt *)this)->memContext;
                break;
            }

            case varTypeUInt64:
            {
                memContext = ((VariantUInt64 *)this)->memContext;
                break;
            }

            case varTypeVariantList:
            {
                memContext = ((VariantVariantList *)this)->memContext;
                varLstFree(((VariantVariantList *)this)->data);
                break;
            }
        }

        MEM_CONTEXT_BEGIN(memContext)
        {
            memFree(this);
        }
        MEM_CONTEXT_END();
    }

    FUNCTION_TEST_RETURN_VOID();
}
