/***********************************************************************************************************************************
Represent Short Strings as Integers

It is often useful to represent identifiers as strings when they cannot easily be represented as an enum/integer, e.g. because they
are distributed among a number of unrelated modules or need to be passed to remote processes. Strings are also more helpful in
debugging since they can be recognized without cross-referencing the source. However, strings are awkward to work with in C since
they cannot be directly used in switch statements leading to less efficient if-else structures.

A StringId encodes a short string into an integer so it can be used in switch statements but may also be readily converted back into
a string for debugging purposes. StringIds may also be suitable for matching user input providing the strings are short enough.

strIdFromStr("mytest0123a") will return the StringId 0x7de75c51315464d5. Using the value, the string representation can be retrieved
strIdToStr(0x7de75c51315464d5) which returns "mytest0123+" where the plus at the end signals that the original string was equal to
or longer than the maximum allowed.

When assigning a StringId to an enum, it will be necessary to cast the StringId to the enum type if the enum contains all 32-bit
values, since some compilers will complain about the implicit conversion without a cast. The enum will be 32-bit if all values of
the enum are <= 0xffffffff.

See bldStrId() for information on generating StringId constants.
***********************************************************************************************************************************/
#ifndef COMMON_TYPE_STRINGID_H
#define COMMON_TYPE_STRINGID_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "common/type/string.h"

/***********************************************************************************************************************************
Maximum number of characters in the string representation of a StringId. When the string representation is equal to or greater than
the MAX, then a plus sign + will be assigned as the last (MAX) character in the string representation. This is a safe buffer size
when calling strIdToZN. If the buffer needs to be zero-terminated then an extra byte should be added.
***********************************************************************************************************************************/
// Maximum for specific encodings (e.g. 5-bit, 6-bit)
#define STRID5_MAX                                                  13
#define STRID6_MAX                                                  11

// Maximum for any encoding
#define STRID_MAX                                                   STRID5_MAX

/***********************************************************************************************************************************
Constants used to extract information from the header
***********************************************************************************************************************************/
#define STRING_ID_BIT_MASK                                          3

/***********************************************************************************************************************************
StringId typedef to make them more recognizable in the code
***********************************************************************************************************************************/
typedef uint64_t StringId;

/***********************************************************************************************************************************
Macros to define constant StringIds. ALWAYS use bldStrId() to create these macros. The parameters in the macros are not verified
against each other so the str parameter is included only for documentation purposes.
***********************************************************************************************************************************/
#define STRID5(str, strId)                                          strId
#define STRID6(str, strId)                                          strId

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Convert N chars to StringId, If the string is longer than the allowable length for the selected encoding then the StringID will
// be marked as "partial" and will have a '+' appended whenever it is converted back to a string. This is to distinguish it from a
// string with the same number of encoded characters that did not overflow.
FN_EXTERN StringId strIdFromZN(const char *buffer, size_t size, bool error);

// Convert String to StringId using strIdFromZN()
FN_INLINE_ALWAYS StringId
strIdFromStr(const String *const str)
{
    return strIdFromZN(strZ(str), strSize(str), true);
}

// Convert zero-terminated string to StringId using strIdFromZN()
FN_INLINE_ALWAYS StringId
strIdFromZ(const char *const str)
{
    return strIdFromZN(str, strlen(str), true);
}

// Write StringId to characters without zero-terminating. The buffer at ptr must have enough space to write the entire StringId,
// which could be eight characters. However, the caller may know the exact (or max length) in advance and act accordingly. The
// actual number of bytes written is returned.
FN_EXTERN size_t strIdToZN(StringId strId, char *const buffer);

// Convert StringId to String
FN_EXTERN String *strIdToStr(const StringId strId);

// Convert StringId to zero-terminated string. See strIdToZN() for buffer sizing and return value.
FN_EXTERN size_t strIdToZ(const StringId strId, char *const buffer);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
FN_EXTERN size_t strIdToLog(StringId strId, char *buffer, size_t bufferSize);

#define FUNCTION_LOG_STRING_ID_TYPE                                                                                                \
    StringId
#define FUNCTION_LOG_STRING_ID_FORMAT(value, buffer, bufferSize)                                                                   \
    strIdToLog(value, buffer, bufferSize)

#endif
