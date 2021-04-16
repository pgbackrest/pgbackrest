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

See strIdGenerate() for information on StringId constants.
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
StringId typedef to make them more recognizable in the code
***********************************************************************************************************************************/
typedef uint64_t StringId;

/***********************************************************************************************************************************
Number of bits to use for encoding. The number of bits affects the character set that can be encoded.
***********************************************************************************************************************************/
typedef enum
{
    stringIdBit5 = 0,                                               // 5-bit encoding for a-z, 1-4, and - characters
    stringIdBit6 = 1,                                               // 6-bit encoding for a-z, 0-9, A-Z, and - characters
} StringIdBit;

/***********************************************************************************************************************************
Macros to define constant StringIds. ALWAYS use strIdGenerate() to create these macros. The parameters in the macros are not
verified against each other so the str parameter is included only for documentation purposes.
***********************************************************************************************************************************/
#define STRID5(str, strId)                                          strId
#define STRID6(str, strId)                                          strId

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Convert N chars to StringId, If the string is longer than the allowable length for the selected encoding then the StringID will
// be marked as "partial" and will have a '+' appended whenever it is converted back to a string. This is to distiguish it from a
// string with the same number of encoded characters that did not overflow.
StringId strIdFromZN(const StringIdBit bit, const char *const buffer, const size_t size);

// Convert String to StringId using strIdFromZN()
__attribute__((always_inline)) static inline StringId
strIdFromStr(const StringIdBit bit, const String *const str)
{
    return strIdFromZN(bit, strZ(str), strSize(str));
}

// Convert zero-terminted string to StringId using strIdFromZN()
__attribute__((always_inline)) static inline StringId
strIdFromZ(const StringIdBit bit, const char *const str)
{
    return strIdFromZN(bit, str, strlen(str));
}

// Write StringId to characters without zero-terminating. The buffer at ptr must have enough space to write the entire StringId,
// which could be eight characters. However, the caller may know the exact (or max length) in advance and act accordingly. The
// actual number of bytes written is returned.
size_t strIdToZN(StringId strId, char *const buffer);

// Convert StringId to String
String *strIdToStr(const StringId strId);

// Convert StringId to zero-terminated string. See strIdToZN() for buffer sizing and return value.
size_t strIdToZ(const StringId strId, char *const buffer);

/***********************************************************************************************************************************
Generate constant StringIds

To generate a constant StringId call strIdGenerate() in any debug build. It will throw an error with the generated StringId macro
in the error message.

For example:

strIdGenerate("test");

will throw the following error message:

STRID5("test", 0xa4cb40)

which can be used in a function, switch, or #define, e.g.:

#define TEST_STRID                                                  STRID5("test", 0xa4cb40)

DO NOT MODIFY either parameter in the macro -- ALWAYS use strIdGenerate() to create a new constant StringId.
***********************************************************************************************************************************/
#ifdef DEBUG
    // Generate a new constant StringId
    void strIdGenerate(const char *const buffer) __attribute__((__noreturn__));
#endif

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
size_t strIdToLog(const StringId strId, char *const buffer, const size_t bufferSize);

#define FUNCTION_LOG_STRING_ID_TYPE                                                                                                \
    StringId
#define FUNCTION_LOG_STRING_ID_FORMAT(value, buffer, bufferSize)                                                                   \
    strIdToLog(value, buffer, bufferSize)

#endif
