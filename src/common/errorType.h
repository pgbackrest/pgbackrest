/***********************************************************************************************************************************
Application-Defined Errors
***********************************************************************************************************************************/
#ifndef ERROR_TYPE_H
#define ERROR_TYPE_H

#include "common/type.h"

// Represents an error type
typedef struct ErrorType ErrorType;

// Macros for declaring and defining new error types
#define ERROR_DECLARE(name)                                                                                                        \
    extern const ErrorType name

// Error types
ERROR_DECLARE(AssertError);

ERROR_DECLARE(FormatError);
ERROR_DECLARE(CommandRequiredError);
ERROR_DECLARE(OptionInvalidError);
ERROR_DECLARE(OptionInvalidValueError);
ERROR_DECLARE(OptionRequiredError);
ERROR_DECLARE(FileOpenError);
ERROR_DECLARE(FileReadError);
ERROR_DECLARE(ParamRequiredError);
ERROR_DECLARE(ArchiveMismatchError);
ERROR_DECLARE(CommandInvalidError);
ERROR_DECLARE(PathOpenError);
ERROR_DECLARE(FileWriteError);
ERROR_DECLARE(ArchiveTimeoutError);
ERROR_DECLARE(MemoryError);
ERROR_DECLARE(CipherError);
ERROR_DECLARE(ParamInvalidError);

ERROR_DECLARE(RuntimeError);

// Functions
int errorTypeCode(const ErrorType *errorType);
const ErrorType *errorTypeFromCode(int code);
const char *errorTypeName(const ErrorType *errorType);
const ErrorType *errorTypeParent(const ErrorType *errorType);
bool errorTypeExtends(const ErrorType *child, const ErrorType *parent);

#endif
