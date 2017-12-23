/***********************************************************************************************************************************
Variant Data Type
***********************************************************************************************************************************/
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "common/errorType.h"
#include "common/memContext.h"
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
    "KeyValue",                                                     // varTypeKeyValue
    "String",                                                       // varTypeString
    "VariantList",                                                  // varTypeVariantList
};

/***********************************************************************************************************************************
New variant of any supported type
***********************************************************************************************************************************/
static Variant *
varNewInternal(VariantType type, void *data, size_t dataSize)
{
    // Allocate memory for the variant and set the type
    Variant *this = memNew(sizeof(Variant) + dataSize);
    this->memContext = memContextCurrent();
    this->type = type;

    // Copy data if there is any
    if (dataSize > 0)
        memcpy((unsigned char *)this + sizeof(Variant), data, dataSize);

    return this;
}

/***********************************************************************************************************************************
Get a pointer to the data stored in the variant.  This hides the complicated pointer arithmetic.
***********************************************************************************************************************************/
static void *
varData(const Variant *this)
{
    return (void *)((unsigned char *)this + sizeof(Variant));
}

/***********************************************************************************************************************************
Duplicate a variant
***********************************************************************************************************************************/
Variant *
varDup(const Variant *this)
{
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

    return result;
}

/***********************************************************************************************************************************
Test if variants are equal
***********************************************************************************************************************************/
bool
varEq(const Variant *this1, const Variant *this2)
{
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

                case varTypeString:
                {
                    result = strEq(varStr(this1), varStr(this2));
                    break;
                }

                default:
                    THROW(AssertError, "unable to test equality for %s", variantTypeName[this1->type]);
            }
        }
    }
    // Else they are equal if they are both null
    else
        result = this1 == NULL && this2 == NULL;

    return result;
}

/***********************************************************************************************************************************
Get variant type
***********************************************************************************************************************************/
VariantType
varType(const Variant *this)
{
    return this->type;
}

/***********************************************************************************************************************************
New bool variant
***********************************************************************************************************************************/
Variant *
varNewBool(bool data)
{
    return varNewInternal(varTypeBool, (void *)&data, sizeof(data));
}

/***********************************************************************************************************************************
Return bool
***********************************************************************************************************************************/
bool
varBool(const Variant *this)
{
    // Only valid for int
    if (this->type != varTypeBool)
        THROW(AssertError, "variant type is not bool");

    // Get the int
    return *((bool *)varData(this));
}

/***********************************************************************************************************************************
Return bool regardless of variant type
***********************************************************************************************************************************/
bool
varBoolForce(const Variant *this)
{
    int result = 0;

    switch (this->type)
    {
        case varTypeBool:
            result = varBool(this);
            break;

        case varTypeInt:
            result = varInt(this) != 0;
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
                THROW(FormatError, "unable to convert str '%s' to bool", string);

            // False if in first half of list, true if in second half
            result = boolIdx / (sizeof(boolString) / sizeof(char *) / 2);

            break;
        }

        default:
            THROW(FormatError, "unable to force %s to %s", variantTypeName[this->type], variantTypeName[varTypeBool]);
    }

    return result;
}

/***********************************************************************************************************************************
New double variant
***********************************************************************************************************************************/
Variant *
varNewDbl(double data)
{
    return varNewInternal(varTypeDouble, (unsigned char *)&data, sizeof(data));
}

/***********************************************************************************************************************************
Return double
***********************************************************************************************************************************/
double
varDbl(const Variant *this)
{
    // Only valid for double
    if (this->type != varTypeDouble)
        THROW(AssertError, "variant type is not double");

    // Get the int
    return *((double *)varData(this));
}

/***********************************************************************************************************************************
Return double regardless of variant type
***********************************************************************************************************************************/
double
varDblForce(const Variant *this)
{
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

        case varTypeString:
        {
            sscanf(strPtr(varStr(this)), "%lf", &result);

            if (result == 0 && strcmp(strPtr(varStr(this)), "0") != 0)
            {
                THROW(
                    FormatError, "unable to force %s '%s' to %s", variantTypeName[this->type], strPtr(varStr(this)),
                    variantTypeName[varTypeDouble]);
            }

            break;
        }

        default:
            THROW(FormatError, "unable to force %s to %s", variantTypeName[this->type], variantTypeName[varTypeDouble]);
    }

    return result;
}

/***********************************************************************************************************************************
New int variant
***********************************************************************************************************************************/
Variant *
varNewInt(int data)
{
    return varNewInternal(varTypeInt, (void *)&data, sizeof(data));
}

/***********************************************************************************************************************************
Return int
***********************************************************************************************************************************/
int
varInt(const Variant *this)
{
    // Only valid for int
    if (this->type != varTypeInt)
        THROW(AssertError, "variant type is not int");

    // Get the int
    return *((int *)varData(this));
}

/***********************************************************************************************************************************
New key/value variant
***********************************************************************************************************************************/
Variant *
varNewKv()
{
    // Create the variant
    KeyValue *data = kvNew();
    return varNewInternal(varTypeKeyValue, (void *)&data, sizeof(data));
}

/***********************************************************************************************************************************
Return key/value
***********************************************************************************************************************************/
KeyValue *
varKv(const Variant *this)
{
    KeyValue *result = NULL;

    if (this != NULL)
    {
        // Only valid for key/value
        if (this->type != varTypeKeyValue)
            THROW(AssertError, "variant type is not 'KeyValue'");

        // Get the string
        result = *((KeyValue **)varData(this));
    }

    return result;
}

/***********************************************************************************************************************************
Return int regardless of variant type
***********************************************************************************************************************************/
int
varIntForce(const Variant *this)
{
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

        case varTypeString:
        {
            result = atoi(strPtr(varStr(this)));

            if (result == 0 && strcmp(strPtr(varStr(this)), "0") != 0)
                THROW(FormatError, "unable to convert str '%s' to int", strPtr(varStr(this)));

            break;
        }

        default:
            THROW(FormatError, "unable to force %s to %s", variantTypeName[this->type], variantTypeName[varTypeInt]);
    }

    return result;
}

/***********************************************************************************************************************************
New string variant
***********************************************************************************************************************************/
Variant *
varNewStr(const String *data)
{
    // Make sure the string is not NULL
    if (data == NULL)
        THROW(AssertError, "string variant cannot be NULL");

    // Create the variant
    String *dataCopy = strDup(data);
    return varNewInternal(varTypeString, (void *)&dataCopy, sizeof(dataCopy));
}

/***********************************************************************************************************************************
New string variant from a zero-terminated string
***********************************************************************************************************************************/
Variant *
varNewStrZ(const char *data)
{
    // Make sure the string is not NULL
    if (data == NULL)
        THROW(AssertError, "zero-terminated string cannot be NULL");

    // Create the variant
    String *dataCopy = strNew(data);
    return varNewInternal(varTypeString, (void *)&dataCopy, sizeof(dataCopy));
}

/***********************************************************************************************************************************
Return string
***********************************************************************************************************************************/
String *
varStr(const Variant *this)
{
    String *result = NULL;

    if (this != NULL)
    {
        // Only valid for strings
        if (this->type != varTypeString)
            THROW(AssertError, "variant type is not string");

        // Get the string
        result = *((String **)varData(this));
    }

    return result;
}

/***********************************************************************************************************************************
New variant list variant
***********************************************************************************************************************************/
Variant *
varNewVarLst(const VariantList *data)
{
    // Create the variant
    VariantList *dataCopy = varLstDup(data);
    return varNewInternal(varTypeVariantList, (void *)&dataCopy, sizeof(dataCopy));
}

/***********************************************************************************************************************************
New empty variant list variant
***********************************************************************************************************************************/
Variant *
varNewVarLstEmpty()
{
    // Create the variant
    VariantList *data = varLstNew();
    return varNewInternal(varTypeVariantList, (void *)&data, sizeof(data));
}

/***********************************************************************************************************************************
Return key/value
***********************************************************************************************************************************/
VariantList *
varVarLst(const Variant *this)
{
    VariantList *result = NULL;

    if (this != NULL)
    {
        // Only valid for key/value
        if (this->type != varTypeVariantList)
            THROW(AssertError, "variant type is not '%s'", variantTypeName[varTypeVariantList]);

        // Get the string
        result = *((VariantList **)varData(this));
    }

    return result;
}

/***********************************************************************************************************************************
Free variant
***********************************************************************************************************************************/
void
varFree(Variant *this)
{
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
                    break;
            }

            memFree(this);
        }
        MEM_CONTEXT_END();
    }
}
