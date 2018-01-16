/***********************************************************************************************************************************
Application-Defined Errors
***********************************************************************************************************************************/
#include "common/error.h"

/***********************************************************************************************************************************
Error code range -- chosen to not overlap with defined return values
***********************************************************************************************************************************/
#define ERROR_CODE_MIN                                              25
#define ERROR_CODE_MAX                                              125

/***********************************************************************************************************************************
Represents an error type
***********************************************************************************************************************************/
struct ErrorType
{
    const int code;
    const char *name;
    const struct ErrorType *parentType;
};

/***********************************************************************************************************************************
Macro to create error structs
***********************************************************************************************************************************/
#define ERROR_DEFINE(code, name, parentType)                                                                                       \
    const ErrorType name = {code, #name, &parentType}

/***********************************************************************************************************************************
Define errors
***********************************************************************************************************************************/
ERROR_DEFINE(ERROR_CODE_MIN, AssertError, RuntimeError);

ERROR_DEFINE(ERROR_CODE_MIN + 04, FormatError, RuntimeError);
ERROR_DEFINE(ERROR_CODE_MIN + 05, CommandRequiredError, FormatError);
ERROR_DEFINE(ERROR_CODE_MIN + 06, OptionInvalidError, RuntimeError);
ERROR_DEFINE(ERROR_CODE_MIN + 07, OptionInvalidValueError, RuntimeError);
ERROR_DEFINE(ERROR_CODE_MIN + 12, OptionRequiredError, RuntimeError);
ERROR_DEFINE(ERROR_CODE_MIN + 16, FileOpenError, RuntimeError);
ERROR_DEFINE(ERROR_CODE_MIN + 17, FileReadError, RuntimeError);
ERROR_DEFINE(ERROR_CODE_MIN + 23, CommandInvalidError, FormatError);
ERROR_DEFINE(ERROR_CODE_MIN + 39, FileWriteError, RuntimeError);
ERROR_DEFINE(ERROR_CODE_MIN + 69, MemoryError, RuntimeError);
ERROR_DEFINE(ERROR_CODE_MIN + 70, CipherError, FormatError);

ERROR_DEFINE(ERROR_CODE_MAX, RuntimeError, RuntimeError);

/***********************************************************************************************************************************
Place errors in an array so they can be found by code
***********************************************************************************************************************************/
static const ErrorType *errorTypeList[] =
{
    &AssertError,

    &FormatError,
    &CommandRequiredError,
    &OptionInvalidError,
    &OptionInvalidValueError,
    &OptionRequiredError,
    &FileOpenError,
    &FileReadError,
    &CommandInvalidError,
    &FileWriteError,
    &MemoryError,
    &CipherError,

    &RuntimeError,

    NULL,
};

/***********************************************************************************************************************************
Error type code
***********************************************************************************************************************************/
int
errorTypeCode(const ErrorType *errorType)
{
    return errorType->code;
}

/***********************************************************************************************************************************
Get error type using a code
***********************************************************************************************************************************/
const ErrorType *
errorTypeFromCode(int code)
{
    // Search for error type by code
    int errorTypeIdx = 0;
    const ErrorType *result = errorTypeList[errorTypeIdx];

    while (result != NULL)
    {
        if (result->code == code)
            break;

        result = errorTypeList[++errorTypeIdx];
    }

    // Error if type was not found
    if (result == NULL)
        THROW(AssertError, "could not find error type for code '%d'", code);

    return result;
}

/***********************************************************************************************************************************
Error type name
***********************************************************************************************************************************/
const char *
errorTypeName(const ErrorType *errorType)
{
    return errorType->name;
}

/***********************************************************************************************************************************
Error type parent
***********************************************************************************************************************************/
const ErrorType *
errorTypeParent(const ErrorType *errorType)
{
    return errorType->parentType;
}

/***********************************************************************************************************************************
Does the child error type extend the parent error type?
***********************************************************************************************************************************/
bool errorTypeExtends(const ErrorType *child, const ErrorType *parent)
{
    // Search for the parent
    for (; child && errorTypeParent(child) != child; child = (ErrorType *)errorTypeParent(child))
    {
        if (errorTypeParent(child) == parent)
            return true;
    }

    // Parent was not found
    return false;
}
