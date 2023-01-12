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
// Used to define Bool Variant constants that will be externed using VARIANT_DECLARE().
#define VARIANT_BOOL_EXTERN(name, dataParam)                                                                                       \
    VR_EXTERN_DEFINE const Variant *const name = ((const Variant *)&(const VariantBoolPub){.type = varTypeBool, .data = dataParam})

VARIANT_BOOL_EXTERN(BOOL_FALSE_VAR,                                 false);
VARIANT_BOOL_EXTERN(BOOL_TRUE_VAR,                                  true);

/***********************************************************************************************************************************
Information about the variant
***********************************************************************************************************************************/
struct Variant
{
    VariantPub pub;                                                 // Publicly accessible variables
};

typedef struct VariantBool
{
    VariantBoolPub pub;                                             // Publicly accessible variables
} VariantBool;

typedef struct VariantInt
{
    VariantIntPub pub;                                              // Publicly accessible variables
} VariantInt;

typedef struct VariantInt64
{
    VariantInt64Pub pub;                                            // Publicly accessible variables
} VariantInt64;

typedef struct VariantKeyValue
{
    VARIANT_COMMON
    KeyValue *data;                                                 // KeyValue data
} VariantKeyValue;

typedef struct VariantString
{
    VariantStringPub pub;                                           // Publicly accessible variables
} VariantString;

typedef struct VariantUInt
{
    VariantUIntPub pub;                                             // Publicly accessible variables
} VariantUInt;

typedef struct VariantUInt64
{
    VariantUInt64Pub pub;                                           // Publicly accessible variables
} VariantUInt64;

typedef struct VariantVariantList
{
    VARIANT_COMMON
    VariantList *data;                                              // VariantList data
} VariantVariantList;

/***********************************************************************************************************************************
Variant type names
***********************************************************************************************************************************/
static const char *const variantTypeName[] =
{
    "bool",                                                         // varTypeBool
    "int",                                                          // varTypeInt
    "int64",                                                        // varTypeInt64
    "KeyValue",                                                     // varTypeKeyValue
    "String",                                                       // varTypeString
    "unsigned int",                                                 // varTypeUInt
    "uint64",                                                       // varTypeUInt64
    "VariantList",                                                  // varTypeVariantList
};

/**********************************************************************************************************************************/
FN_EXTERN Variant *
varDup(const Variant *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(VARIANT, this);
    FUNCTION_TEST_END();

    Variant *result = NULL;

    if (this != NULL)
    {
        switch (varType(this))
        {
            case varTypeBool:
                result = varNewBool(varBool(this));
                break;

            case varTypeInt:
                result = varNewInt(varInt(this));
                break;

            case varTypeInt64:
                result = varNewInt64(varInt64(this));
                break;

            case varTypeKeyValue:
            {
                result = varNewKv(kvDup(varKv(this)));
                break;
            }

            case varTypeString:
                result = varNewStr(varStr(this));
                break;

            case varTypeUInt:
                result = varNewUInt(varUInt(this));
                break;

            case varTypeUInt64:
                result = varNewUInt64(varUInt64(this));
                break;

            case varTypeVariantList:
                result = varNewVarLst(varVarLst(this));
                break;
        }
    }

    FUNCTION_TEST_RETURN(VARIANT, result);
}

/**********************************************************************************************************************************/
FN_EXTERN bool
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
                    result = varBool(this1) == varBool(this2);
                    break;

                case varTypeInt:
                    result = varInt(this1) == varInt(this2);
                    break;

                case varTypeInt64:
                    result = varInt64(this1) == varInt64(this2);
                    break;

                case varTypeString:
                    result = strEq(varStr(this1), varStr(this2));
                    break;

                case varTypeUInt:
                    result = varUInt(this1) == varUInt(this2);
                    break;

                case varTypeUInt64:
                    result = varUInt64(this1) == varUInt64(this2);
                    break;

                default:
                    THROW_FMT(AssertError, "unable to test equality for %s", variantTypeName[varType(this1)]);
            }
        }
    }
    // Else they are equal if they are both null
    else
        result = this1 == NULL && this2 == NULL;

    FUNCTION_TEST_RETURN(BOOL, result);
}

/**********************************************************************************************************************************/
FN_EXTERN Variant *
varNewBool(bool data)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(BOOL, data);
    FUNCTION_TEST_END();

    VariantBool *this = NULL;

    OBJ_NEW_BEGIN(VariantBool)
    {
        this = OBJ_NAME(OBJ_NEW_ALLOC(), Variant::VariantBool);

        *this = (VariantBool)
        {
            .pub =
            {
                .type = varTypeBool,
                .data = data,
            },
        };
    }
    OBJ_NEW_END();

    FUNCTION_TEST_RETURN(VARIANT, (Variant *)this);
}

/**********************************************************************************************************************************/
FN_EXTERN bool
varBool(const Variant *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(VARIANT, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(varType(this) == varTypeBool);

    FUNCTION_TEST_RETURN(BOOL, ((VariantBool *)this)->pub.data);
}

FN_EXTERN bool
varBoolForce(const Variant *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(VARIANT, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    bool result = false;

    switch (varType(this))
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
            const char *string = strZ(varStr(this));
            unsigned int boolIdx;

            for (boolIdx = 0; boolIdx < LENGTH_OF(boolString); boolIdx++)
                if (strcasecmp(string, boolString[boolIdx]) == 0)
                    break;

            // If string was not found then not a boolean
            if (boolIdx == LENGTH_OF(boolString))
                THROW_FMT(FormatError, "unable to convert str '%s' to bool", string);

            // False if in first half of list, true if in second half
            result = boolIdx / (LENGTH_OF(boolString) / 2);

            break;
        }

        case varTypeUInt:
            result = varUInt(this) != 0;
            break;

        case varTypeUInt64:
            result = varUInt64(this) != 0;
            break;

        default:
            THROW_FMT(AssertError, "unable to force %s to %s", variantTypeName[varType(this)], variantTypeName[varTypeBool]);
    }

    FUNCTION_TEST_RETURN(BOOL, result);
}

/**********************************************************************************************************************************/
FN_EXTERN Variant *
varNewInt(int data)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(INT, data);
    FUNCTION_TEST_END();

    VariantInt *this = NULL;

    OBJ_NEW_BEGIN(VariantInt)
    {
        this = OBJ_NAME(OBJ_NEW_ALLOC(), Variant::VariantInt);

        *this = (VariantInt)
        {
            .pub =
            {
                .type = varTypeInt,
                .data = data,
            },
        };
    }
    OBJ_NEW_END();

    FUNCTION_TEST_RETURN(VARIANT, (Variant *)this);
}

/**********************************************************************************************************************************/
FN_EXTERN int
varInt(const Variant *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(VARIANT, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(varType(this) == varTypeInt);

    FUNCTION_TEST_RETURN(INT, ((VariantInt *)this)->pub.data);
}

FN_EXTERN int
varIntForce(const Variant *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(VARIANT, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    int result = 0;

    switch (varType(this))
    {
        case varTypeBool:
            result = varBool(this);
            break;

        case varTypeInt:
            result = varInt(this);
            break;

        case varTypeInt64:
        {
            int64_t resultTest = varInt64(this);

            // Make sure the value fits into a normal 32-bit int range since 32-bit platforms are supported
            if (resultTest > INT32_MAX || resultTest < INT32_MIN)
                THROW_FMT(
                    FormatError, "unable to convert %s %" PRId64 " to %s", variantTypeName[varType(this)], resultTest,
                    variantTypeName[varTypeInt]);

            result = (int)resultTest;
            break;
        }

        case varTypeString:
            result = cvtZToInt(strZ(varStr(this)));
            break;

        case varTypeUInt:
        {
            unsigned int resultTest = varUInt(this);

            // Make sure the value fits into a normal 32-bit int range
            if (resultTest > INT32_MAX)
                THROW_FMT(
                    FormatError, "unable to convert %s %u to %s", variantTypeName[varType(this)], resultTest,
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
                    FormatError, "unable to convert %s %" PRIu64 " to %s", variantTypeName[varType(this)], resultTest,
                    variantTypeName[varTypeInt]);

            result = (int)resultTest;
            break;
        }

        default:
            THROW_FMT(AssertError, "unable to force %s to %s", variantTypeName[varType(this)], variantTypeName[varTypeInt]);
    }

    FUNCTION_TEST_RETURN(INT, result);
}

/**********************************************************************************************************************************/
FN_EXTERN Variant *
varNewInt64(int64_t data)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(INT64, data);
    FUNCTION_TEST_END();

    VariantInt64 *this = NULL;

    OBJ_NEW_BEGIN(VariantInt64)
    {
        this = OBJ_NAME(OBJ_NEW_ALLOC(), Variant::VariantInt64);

        *this = (VariantInt64)
        {
            .pub =
            {
                .type = varTypeInt64,
                .data = data,
            },
        };
    }
    OBJ_NEW_END();

    FUNCTION_TEST_RETURN(VARIANT, (Variant *)this);
}

/**********************************************************************************************************************************/
FN_EXTERN int64_t
varInt64(const Variant *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(VARIANT, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(varType(this) == varTypeInt64);

    FUNCTION_TEST_RETURN(INT64, ((VariantInt64 *)this)->pub.data);
}

FN_EXTERN int64_t
varInt64Force(const Variant *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(VARIANT, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    int64_t result = 0;

    switch (varType(this))
    {
        case varTypeBool:
            result = varBool(this);
            break;

        case varTypeInt:
            result = (int64_t)varInt(this);
            break;

        case varTypeInt64:
            result = varInt64(this);
            break;

        case varTypeString:
            result = cvtZToInt64(strZ(varStr(this)));
            break;

        case varTypeUInt:
            result = varUInt(this);

            break;

        case varTypeUInt64:
        {
            uint64_t resultTest = varUInt64(this);

            // If max number of unsigned 64-bit integer is greater than max 64-bit signed integer can hold, then error
            if (resultTest <= INT64_MAX)
                result = (int64_t)resultTest;
            else
            {
                THROW_FMT(
                    FormatError, "unable to convert %s %" PRIu64 " to %s", variantTypeName[varType(this)], resultTest,
                    variantTypeName[varTypeInt64]);
            }

            break;
        }

        default:
            THROW_FMT(AssertError, "unable to force %s to %s", variantTypeName[varType(this)], variantTypeName[varTypeInt64]);
    }

    FUNCTION_TEST_RETURN(INT64, result);
}

/**********************************************************************************************************************************/
FN_EXTERN Variant *
varNewUInt(unsigned int data)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(UINT, data);
    FUNCTION_TEST_END();

    VariantUInt *this = NULL;

    OBJ_NEW_BEGIN(VariantUInt)
    {
        this = OBJ_NAME(OBJ_NEW_ALLOC(), Variant::VariantUInt);

        *this = (VariantUInt)
        {
            .pub =
            {
                .type = varTypeUInt,
                .data = data,
            },
        };
    }
    OBJ_NEW_END();

    FUNCTION_TEST_RETURN(VARIANT, (Variant *)this);
}

/**********************************************************************************************************************************/
FN_EXTERN unsigned int
varUInt(const Variant *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(VARIANT, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(varType(this) == varTypeUInt);

    FUNCTION_TEST_RETURN(UINT, ((VariantUInt *)this)->pub.data);
}

FN_EXTERN unsigned int
varUIntForce(const Variant *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(VARIANT, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    unsigned int result = 0;

    switch (varType(this))
    {
        case varTypeBool:
            result = varBool(this);
            break;

        case varTypeInt:
        {
            int resultTest = varInt(this);

            // If integer is a negative number, throw an error since the resulting conversion would be a different number
            if (resultTest >= 0)
                result = (unsigned int)resultTest;
            else
            {
                THROW_FMT(
                    FormatError, "unable to convert %s %d to %s", variantTypeName[varType(this)], resultTest,
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
                    FormatError, "unable to convert %s %" PRId64 " to %s", variantTypeName[varType(this)], resultTest,
                    variantTypeName[varTypeUInt]);
            }

            break;
        }

        case varTypeUInt:
            result = varUInt(this);
            break;

        case varTypeUInt64:
        {
            uint64_t resultTest = varUInt64(this);

            // If integer is too large, throw an error since the resulting conversion would be out of bounds
            if (resultTest <= UINT_MAX)
                result = (unsigned int)resultTest;
            else
            {
                THROW_FMT(
                    FormatError, "unable to convert %s %" PRIu64 " to %s", variantTypeName[varType(this)], resultTest,
                    variantTypeName[varTypeUInt]);
            }

            break;
        }

        case varTypeString:
            result = cvtZToUInt(strZ(varStr(this)));
            break;

        default:
            THROW_FMT(AssertError, "unable to force %s to %s", variantTypeName[varType(this)], variantTypeName[varTypeUInt]);
    }

    FUNCTION_TEST_RETURN(UINT, result);
}

/**********************************************************************************************************************************/
FN_EXTERN Variant *
varNewUInt64(uint64_t data)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(UINT64, data);
    FUNCTION_TEST_END();

    VariantUInt64 *this = NULL;

    OBJ_NEW_BEGIN(VariantUInt64)
    {
        this = OBJ_NAME(OBJ_NEW_ALLOC(), Variant::VariantUInt64);

        *this = (VariantUInt64)
        {
            .pub =
            {
                .type = varTypeUInt64,
                .data = data,
            },
        };
    }
    OBJ_NEW_END();

    FUNCTION_TEST_RETURN(VARIANT, (Variant *)this);
}

/**********************************************************************************************************************************/
FN_EXTERN uint64_t
varUInt64(const Variant *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(VARIANT, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(varType(this) == varTypeUInt64);

    FUNCTION_TEST_RETURN(UINT64, ((VariantUInt64 *)this)->pub.data);
}

FN_EXTERN uint64_t
varUInt64Force(const Variant *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(VARIANT, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    uint64_t result = 0;

    switch (varType(this))
    {
        case varTypeBool:
            result = varBool(this);
            break;

        case varTypeInt:
        {
            int resultTest = varInt(this);

            // If integer is a negative number, throw an error since the resulting conversion would be a different number
            if (resultTest >= 0)
                result = (uint64_t)resultTest;
            else
            {
                THROW_FMT(
                    FormatError, "unable to convert %s %d to %s", variantTypeName[varType(this)], resultTest,
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
                    FormatError, "unable to convert %s %" PRId64 " to %s", variantTypeName[varType(this)], resultTest,
                    variantTypeName[varTypeUInt64]);
            }

            break;
        }

        case varTypeString:
            result = cvtZToUInt64(strZ(varStr(this)));
            break;

        case varTypeUInt:
            result = varUInt(this);
            break;

        case varTypeUInt64:
            result = varUInt64(this);
            break;

        default:
            THROW_FMT(AssertError, "unable to force %s to %s", variantTypeName[varType(this)], variantTypeName[varTypeUInt64]);
    }

    FUNCTION_TEST_RETURN(UINT64, result);
}

/**********************************************************************************************************************************/
FN_EXTERN Variant *
varNewKv(KeyValue *data)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(KEY_VALUE, data);
    FUNCTION_TEST_END();

    VariantKeyValue *this = NULL;

    OBJ_NEW_BEGIN(VariantKeyValue, .childQty = 1)
    {
        this = OBJ_NAME(OBJ_NEW_ALLOC(), Variant::VariantKeyValue);

        *this = (VariantKeyValue)
        {
            .type = varTypeKeyValue,
        };

        if (data != NULL)
            this->data = kvMove(data, memContextCurrent());
    }
    OBJ_NEW_END();

    FUNCTION_TEST_RETURN(VARIANT, (Variant *)this);
}

/**********************************************************************************************************************************/
FN_EXTERN KeyValue *
varKv(const Variant *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(VARIANT, this);
    FUNCTION_TEST_END();

    KeyValue *result = NULL;

    if (this != NULL)
    {
        ASSERT(varType(this) == varTypeKeyValue);
        result = ((VariantKeyValue *)this)->data;
    }

    FUNCTION_TEST_RETURN(KEY_VALUE, result);
}

/**********************************************************************************************************************************/
FN_EXTERN Variant *
varNewStr(const String *data)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, data);
    FUNCTION_TEST_END();

    VariantString *this = NULL;

    // If the variant is larger than the extra allowed with a mem context then allocate the buffer separately
    size_t allocExtra = sizeof(VariantString) + (data != NULL ? sizeof(StringPub) + strSize(data) + 1 : 0);

    if (allocExtra > MEM_CONTEXT_ALLOC_EXTRA_MAX)
    {
        ASSERT(data != NULL);

        OBJ_NEW_BEGIN(VariantString, .childQty = 1)
        {
            this = OBJ_NAME(OBJ_NEW_ALLOC(), Variant::VariantString);

            *this = (VariantString)
            {
                .pub =
                {
                    .type = varTypeString,
                    .data = strDup(data),
                },
            };
        }
        OBJ_NEW_END();

        FUNCTION_TEST_RETURN(VARIANT, (Variant *)this);
    }

    OBJ_NEW_EXTRA_BEGIN(VariantString, (uint16_t)(allocExtra))
    {
        this = OBJ_NAME(OBJ_NEW_ALLOC(), Variant::VariantString);

        *this = (VariantString)
        {
            .pub =
            {
                .type = varTypeString,
            },
        };

        if (data != NULL)
        {
            // Point the String after the VariantString struct
            StringPub *const pubData = (StringPub *)(this + 1);
            this->pub.data = (String *)pubData;

            // Assign the string size and buffer (after StringPub struct)
            *pubData = (StringPub)
            {
                .size = (unsigned int)strSize(data),
                .buffer = (char *)(pubData + 1),
            };

            // Assign the string
            memcpy(pubData->buffer, strZ(data), strSize(data));
            pubData->buffer[strSize(data)] = '\0';
        }
    }
    OBJ_NEW_END();

    FUNCTION_TEST_RETURN(VARIANT, (Variant *)this);
}

FN_EXTERN Variant *
varNewStrZ(const char *data)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRINGZ, data);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RETURN(VARIANT, varNewStr(data == NULL ? NULL : STR(data)));
}

/**********************************************************************************************************************************/
FN_EXTERN const String *
varStr(const Variant *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(VARIANT, this);
    FUNCTION_TEST_END();

    String *result = NULL;

    if (this != NULL)
    {
        ASSERT(varType(this) == varTypeString);
        result = ((VariantString *)this)->pub.data;
    }

    FUNCTION_TEST_RETURN(STRING, result);
}

FN_EXTERN String *
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
            result = strNewZ(cvtBoolToConstZ(varBool(this)));
            break;

        case varTypeInt:
        {
            char working[CVT_BASE10_BUFFER_SIZE];

            cvtIntToZ(varInt(this), working, sizeof(working));
            result = strNewZ(working);
            break;
        }

        case varTypeInt64:
        {
            char working[CVT_BASE10_BUFFER_SIZE];

            cvtInt64ToZ(varInt64(this), working, sizeof(working));
            result = strNewZ(working);
            break;
        }

        case varTypeString:
            result = strDup(varStr(this));
            break;

        case varTypeUInt:
        {
            char working[CVT_BASE10_BUFFER_SIZE];

            cvtUIntToZ(varUInt(this), working, sizeof(working));
            result = strNewZ(working);
            break;
        }

        case varTypeUInt64:
        {
            char working[CVT_BASE10_BUFFER_SIZE];

            cvtUInt64ToZ(varUInt64(this), working, sizeof(working));
            result = strNewZ(working);
            break;
        }

        default:
            THROW_FMT(FormatError, "unable to force %s to %s", variantTypeName[varType(this)], variantTypeName[varTypeString]);
    }

    FUNCTION_TEST_RETURN(STRING, result);
}

/**********************************************************************************************************************************/
FN_EXTERN Variant *
varNewVarLst(const VariantList *data)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(VARIANT_LIST, data);
    FUNCTION_TEST_END();

    VariantVariantList *this = NULL;

    OBJ_NEW_BEGIN(VariantVariantList, .childQty = 1)
    {
        this = OBJ_NAME(OBJ_NEW_ALLOC(), Variant::VariantVariantList);

        *this = (VariantVariantList)
        {
            .type = varTypeVariantList,
        };

        if (data != NULL)
            this->data = varLstDup(data);
    }
    OBJ_NEW_END();

    FUNCTION_TEST_RETURN(VARIANT, (Variant *)this);
}

/**********************************************************************************************************************************/
FN_EXTERN VariantList *
varVarLst(const Variant *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(VARIANT, this);
    FUNCTION_TEST_END();

    VariantList *result = NULL;

    if (this != NULL)
    {
        ASSERT(varType(this) == varTypeVariantList);
        result = ((VariantVariantList *)this)->data;
    }

    FUNCTION_TEST_RETURN(VARIANT_LIST, result);
}

/**********************************************************************************************************************************/
FN_EXTERN void
varToLog(const Variant *const this, StringStatic *const debugLog)
{
    switch (varType(this))
    {
        case varTypeString:
            strStcResultSizeInc(
                debugLog,
                FUNCTION_LOG_OBJECT_FORMAT(varStr(this), strToLog, strStcRemains(debugLog), strStcRemainsSize(debugLog)));
            break;

        case varTypeKeyValue:
            strStcCat(debugLog, "{KeyValue}");
            break;

        case varTypeVariantList:
            strStcCat(debugLog, "{VariantList}");
            break;

        case varTypeBool:
            strStcFmt(debugLog, "{%s}", cvtBoolToConstZ(varBool(this)));
            break;

        case varTypeInt:
            strStcFmt(debugLog, "{%d}", varInt(this));
            break;

        case varTypeInt64:
            strStcFmt(debugLog, "{%" PRId64 "}", varInt64(this));
            break;

        case varTypeUInt:
            strStcFmt(debugLog, "{%u}", varUInt(this));
            break;

        case varTypeUInt64:
            strStcFmt(debugLog, "{%" PRIu64 "}", varUInt64(this));
            break;
    }
}
