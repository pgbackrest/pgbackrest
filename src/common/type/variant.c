/***********************************************************************************************************************************
Variant Data Type
***********************************************************************************************************************************/
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
Information about the variant
***********************************************************************************************************************************/
struct Variant
{
    MemContext *memContext;                                         // Mem context
    unsigned int type:3;                                            // Variant Type
};

/***********************************************************************************************************************************
Variant type names
***********************************************************************************************************************************/
static const char *variantTypeName[] =
{
    "bool",                                                         // varTypeBool
    "double",                                                       // varTypeDouble,
    "int",                                                          // varTypeInt
    "int64",                                                        // varTypeInt64
    "KeyValue",                                                     // varTypeKeyValue
    "String",                                                       // varTypeString
    "VariantList",                                                  // varTypeVariantList
    "uint64",                                                       // varTypeUInt64
};

/***********************************************************************************************************************************
New variant of any supported type
***********************************************************************************************************************************/
static Variant *
varNewInternal(VariantType type, void *data, size_t dataSize)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, type);
        FUNCTION_TEST_PARAM_P(VOID, data);
        FUNCTION_TEST_PARAM(SIZE, dataSize);
    FUNCTION_TEST_END();

    ASSERT(data != NULL);
    ASSERT(dataSize > 0);

    // Allocate memory for the variant and set the type
    Variant *this = memNew(sizeof(Variant) + dataSize);
    this->memContext = memContextCurrent();
    this->type = type;

    // Copy data
    memcpy((unsigned char *)this + sizeof(Variant), data, dataSize);

    FUNCTION_TEST_RETURN(this);
}

/***********************************************************************************************************************************
Get a pointer to the data stored in the variant.  This hides the complicated pointer arithmetic.
***********************************************************************************************************************************/
static void *
varData(const Variant *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(VARIANT, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN((void *)((unsigned char *)this + sizeof(Variant)));
}

/***********************************************************************************************************************************
Duplicate a variant
***********************************************************************************************************************************/
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

            case varTypeUInt64:
            {
                result = varNewUInt64(varUInt64(this));
                break;
            }

            case varTypeKeyValue:
            {
                KeyValue *data = kvDup(varKv(this));
                result = varNewInternal(varTypeKeyValue, (void *)&data, sizeof(data));
                break;
            }

            case varTypeString:
            {
                result = varNewStr(varStr(this));
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

/***********************************************************************************************************************************
Test if variants are equal
***********************************************************************************************************************************/
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

                case varTypeUInt64:
                {
                    result = varUInt64(this1) == varUInt64(this2);
                    break;
                }

                case varTypeString:
                {
                    result = strEq(varStr(this1), varStr(this2));
                    break;
                }

                case varTypeKeyValue:
                case varTypeVariantList:
                    THROW_FMT(AssertError, "unable to test equality for %s", variantTypeName[this1->type]);
            }
        }
    }
    // Else they are equal if they are both null
    else
        result = this1 == NULL && this2 == NULL;

    FUNCTION_TEST_RETURN(result);
}

/***********************************************************************************************************************************
Get variant type
***********************************************************************************************************************************/
VariantType
varType(const Variant *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(VARIANT, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->type);
}

/***********************************************************************************************************************************
New bool variant
***********************************************************************************************************************************/
Variant *
varNewBool(bool data)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(BOOL, data);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RETURN(varNewInternal(varTypeBool, (void *)&data, sizeof(data)));
}

/***********************************************************************************************************************************
Return bool
***********************************************************************************************************************************/
bool
varBool(const Variant *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(VARIANT, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    ASSERT(this->type == varTypeBool);

    FUNCTION_TEST_RETURN(*((bool *)varData(this)));
}

/***********************************************************************************************************************************
Return bool regardless of variant type
***********************************************************************************************************************************/
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

        case varTypeUInt64:
            result = varUInt64(this) != 0;
            break;

        case varTypeString:
        {
            // List of false/true boolean string values.  Note that false/true values must be equal.
            static const char *boolString[] =
            {
                "n", "f", "0",  "no", "false", "off",
                "y", "t", "1", "yes",  "true",  "on",
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

        default:
            THROW_FMT(AssertError, "unable to force %s to %s", variantTypeName[this->type], variantTypeName[varTypeBool]);
    }

    FUNCTION_TEST_RETURN(result);
}

/***********************************************************************************************************************************
New double variant
***********************************************************************************************************************************/
Variant *
varNewDbl(double data)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(DOUBLE, data);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RETURN(varNewInternal(varTypeDouble, (unsigned char *)&data, sizeof(data)));
}

/***********************************************************************************************************************************
Return double
***********************************************************************************************************************************/
double
varDbl(const Variant *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(VARIANT, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    ASSERT(this->type == varTypeDouble);

    FUNCTION_TEST_RETURN(*((double *)varData(this)));
}

/***********************************************************************************************************************************
Return double regardless of variant type
***********************************************************************************************************************************/
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

        case varTypeUInt64:
        {
            result = (double)varUInt64(this);
            break;
        }

        case varTypeString:
        {
            result = cvtZToDouble(strPtr(varStr(this)));
            break;
        }

        default:
            THROW_FMT(AssertError, "unable to force %s to %s", variantTypeName[this->type], variantTypeName[varTypeDouble]);
    }

    FUNCTION_TEST_RETURN(result);
}

/***********************************************************************************************************************************
New int variant
***********************************************************************************************************************************/
Variant *
varNewInt(int data)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(INT, data);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RETURN(varNewInternal(varTypeInt, (void *)&data, sizeof(data)));
}

/***********************************************************************************************************************************
Return int
***********************************************************************************************************************************/
int
varInt(const Variant *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(VARIANT, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    ASSERT(this->type == varTypeInt);

    FUNCTION_TEST_RETURN(*((int *)varData(this)));
}

/***********************************************************************************************************************************
Return int regardless of variant type
***********************************************************************************************************************************/
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

        case varTypeString:
        {
            result = cvtZToInt(strPtr(varStr(this)));
            break;
        }

        default:
            THROW_FMT(AssertError, "unable to force %s to %s", variantTypeName[this->type], variantTypeName[varTypeInt]);
    }

    FUNCTION_TEST_RETURN(result);
}

/***********************************************************************************************************************************
New int64 variant
***********************************************************************************************************************************/
Variant *
varNewInt64(int64_t data)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(INT64, data);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RETURN(varNewInternal(varTypeInt64, (void *)&data, sizeof(data)));
}

/***********************************************************************************************************************************
Return int64
***********************************************************************************************************************************/
int64_t
varInt64(const Variant *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(VARIANT, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    ASSERT(this->type == varTypeInt64);

    FUNCTION_TEST_RETURN(*((int64_t *)varData(this)));
}

/***********************************************************************************************************************************
Return int64 regardless of variant type
***********************************************************************************************************************************/
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

        case varTypeString:
        {
            result = cvtZToInt64(strPtr(varStr(this)));
            break;
        }

        default:
            THROW_FMT(AssertError, "unable to force %s to %s", variantTypeName[this->type], variantTypeName[varTypeInt64]);
    }

    FUNCTION_TEST_RETURN(result);
}

/***********************************************************************************************************************************
New uint64 variant
***********************************************************************************************************************************/
Variant *
varNewUInt64(uint64_t data)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(UINT64, data);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RETURN(varNewInternal(varTypeUInt64, (void *)&data, sizeof(data)));
}

/***********************************************************************************************************************************
Return int64
***********************************************************************************************************************************/
uint64_t
varUInt64(const Variant *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(VARIANT, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    ASSERT(this->type == varTypeUInt64);

    FUNCTION_TEST_RETURN(*((uint64_t *)varData(this)));
}

/***********************************************************************************************************************************
Return uint64 regardless of variant type
***********************************************************************************************************************************/
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

        case varTypeUInt64:
        {
            result = varUInt64(this);
            break;
        }

        case varTypeString:
        {
            result = cvtZToUInt64(strPtr(varStr(this)));
            break;
        }

        default:
            THROW_FMT(AssertError, "unable to force %s to %s", variantTypeName[this->type], variantTypeName[varTypeUInt64]);
    }

    FUNCTION_TEST_RETURN(result);
}

/***********************************************************************************************************************************
New key/value variant
***********************************************************************************************************************************/
Variant *
varNewKv(void)
{
    FUNCTION_TEST_VOID();

    // Create a new kv for the variant
    KeyValue *data = kvNew();

    FUNCTION_TEST_RETURN(varNewInternal(varTypeKeyValue, (void *)&data, sizeof(data)));
}

/***********************************************************************************************************************************
Return key/value
***********************************************************************************************************************************/
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
        result = *((KeyValue **)varData(this));
    }

    FUNCTION_TEST_RETURN(result);
}

/***********************************************************************************************************************************
New string variant
***********************************************************************************************************************************/
Variant *
varNewStr(const String *data)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, data);
    FUNCTION_TEST_END();

    // Create a copy of the string for the variant
    String *dataCopy = strDup(data);

    FUNCTION_TEST_RETURN(varNewInternal(varTypeString, (void *)&dataCopy, sizeof(dataCopy)));
}

/***********************************************************************************************************************************
New string variant from a zero-terminated string
***********************************************************************************************************************************/
Variant *
varNewStrZ(const char *data)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRINGZ, data);
    FUNCTION_TEST_END();

    // Create a string for the variant
    String *dataCopy = NULL;

    if (data != NULL)
        dataCopy = strNew(data);

    FUNCTION_TEST_RETURN(varNewInternal(varTypeString, (void *)&dataCopy, sizeof(dataCopy)));
}

/***********************************************************************************************************************************
Return string
***********************************************************************************************************************************/
String *
varStr(const Variant *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(VARIANT, this);
    FUNCTION_TEST_END();

    String *result = NULL;

    if (this != NULL)
    {
        ASSERT(this->type == varTypeString);
        result = *((String **)varData(this));
    }

    FUNCTION_TEST_RETURN(result);
}

/***********************************************************************************************************************************
Return string regardless of variant type
***********************************************************************************************************************************/
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
            char working[6];

            cvtBoolToZ(varBool(this), working, sizeof(working));
            result = strNew(working);

            break;
        }

        case varTypeDouble:
        {
            char working[64];

            cvtDoubleToZ(varDbl(this), working, sizeof(working));
            result = strNew(working);

            break;
        }

        case varTypeInt:
        {
            char working[64];

            cvtIntToZ(varInt(this), working, sizeof(working));
            result = strNew(working);
            break;
        }

        case varTypeInt64:
        {
            char working[64];

            cvtInt64ToZ(varInt64(this), working, sizeof(working));
            result = strNew(working);
            break;
        }

        case varTypeUInt64:
        {
            result = strNewFmt("%" PRIu64, varUInt64(this));
            break;
        }

        case varTypeString:
        {
            result = strDup(varStr(this));
            break;
        }

        case varTypeKeyValue:
        case varTypeVariantList:
            THROW_FMT(FormatError, "unable to force %s to %s", variantTypeName[this->type], variantTypeName[varTypeString]);
    }

    FUNCTION_TEST_RETURN(result);
}

/***********************************************************************************************************************************
New variant list variant
***********************************************************************************************************************************/
Variant *
varNewVarLst(const VariantList *data)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(VARIANT_LIST, data);
    FUNCTION_TEST_END();

    VariantList *dataCopy = NULL;

    // Create a copy of the variant list if it is not null
    if (data != NULL)
        dataCopy = varLstDup(data);

    FUNCTION_TEST_RETURN(varNewInternal(varTypeVariantList, (void *)&dataCopy, sizeof(dataCopy)));
}

/***********************************************************************************************************************************
Return key/value
***********************************************************************************************************************************/
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
        result = *((VariantList **)varData(this));
    }

    FUNCTION_TEST_RETURN(result);
}

/***********************************************************************************************************************************
Convert variant to a zero-terminated string for logging
***********************************************************************************************************************************/
String *
varToLog(const Variant *this)
{
    String *result = NULL;

    if (this == NULL)
        result = strNew("null");
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
            case varTypeUInt64:
            {
                result = strNewFmt("{%s}", strPtr(varStrForce(this)));
                break;
            }
        }
    }

    return result;
}

/***********************************************************************************************************************************
Free variant
***********************************************************************************************************************************/
void
varFree(Variant *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(VARIANT, this);
    FUNCTION_TEST_END();

    if (this != NULL)
    {
        MEM_CONTEXT_BEGIN(this->memContext)
        {
            switch (this->type)
            {
                case varTypeKeyValue:
                {
                    kvFree(varKv(this));
                    break;
                }

                case varTypeString:
                {
                    strFree(varStr(this));
                    break;
                }

                case varTypeVariantList:
                {
                    varLstFree(varVarLst(this));
                    break;
                }

                // Nothing additional to free for these types
                case varTypeBool:
                case varTypeDouble:
                case varTypeInt:
                case varTypeInt64:
                case varTypeUInt64:
                    break;
            }

            memFree(this);
        }
        MEM_CONTEXT_END();
    }

    FUNCTION_TEST_RETURN_VOID();
}
