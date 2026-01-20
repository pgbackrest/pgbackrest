/***********************************************************************************************************************************
Represent Short Strings as Integers

It is often useful to represent identifiers as strings when they cannot easily be represented as an enum/integer, e.g. because they
are distributed among a number of unrelated modules or need to be passed to remote processes. Strings are also more helpful in
debugging since they can be recognized without cross-referencing the source. However, strings are awkward to work with in C since
they cannot be directly used in switch statements leading to less efficient if-else structures.

A StringId encodes a short string into an integer so it can be used in switch statements but may also be readily converted back into
a string for debugging purposes. StringIds may also be suitable for matching user input providing the strings are short enough.

strIdFromZ("mytest0123") will return the StringId 0x7de75c51315464d1. Using the value, the string representation can be retrieved
with strIdToZ(0x7de75c51315464d1, ...) which returns "mytest0123+" where the plus at the end signals that the original string was
equal to or longer than the maximum allowed.

When assigning a StringId to an enum, it will be necessary to cast the StringId to the enum type if the enum contains all 32-bit
values, since some compilers will complain about the implicit conversion without a cast. The enum will be 32-bit if all values of
the enum are <= 0xffffffff.

See bldStrId() for information on generating StringId constants.
***********************************************************************************************************************************/
#ifndef COMMON_TYPE_STRINGID_H
#define COMMON_TYPE_STRINGID_H

#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

/***********************************************************************************************************************************
Maximum number of characters in the string representation of a StringId. When the string representation is equal to or greater than
the MAX, then a plus sign + will be assigned as the last (MAX) character in the string representation. This is a safe buffer size
when calling strIdToZN. If the buffer needs to be zero-terminated then an extra byte should be added.
***********************************************************************************************************************************/
// Maximum for specific encodings (e.g. 5-bit, 6-bit)
#define STRID5_MAX                                                  12
#define STRID6_MAX                                                  10

// Maximum for any encoding
#define STRID_MAX                                                   STRID5_MAX

/***********************************************************************************************************************************
Constants used to extract information from the header
***********************************************************************************************************************************/
#define STRING_ID_BIT_MASK                                          3

/***********************************************************************************************************************************
Indicator for no sequence value
***********************************************************************************************************************************/
#define STRING_ID_SEQ_NONE                                          UINT_MAX

/***********************************************************************************************************************************
StringId typedef to make them more recognizable in the code
***********************************************************************************************************************************/
typedef uint64_t StringId;

/***********************************************************************************************************************************
Macros to define constant StringIds. ALWAYS use bldStrId() to create these macros. The parameters in the macros are not verified
against each other so the str parameter is included only for documentation purposes.
***********************************************************************************************************************************/
#define STRID5(str, strId)                                          strId
#define STRID5S(str, seq, strId)                                    strId
#define STRID6(str, strId)                                          strId
#define STRID6S(str, seq, strId)                                    strId

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Convert N chars to StringId and optionally add sequence
FN_EXTERN StringId strIdFromZN(const char *buffer, size_t size, unsigned int sequence);

// Convert zero-terminated string to StringId and add sequence
FN_INLINE_ALWAYS StringId
strIdSeqFromZ(const char *const str, const unsigned int sequence)
{
    return strIdFromZN(str, strlen(str), sequence);
}

// Convert zero-terminated string to StringId
FN_INLINE_ALWAYS StringId
strIdFromZ(const char *const str)
{
    return strIdSeqFromZ(str, STRING_ID_SEQ_NONE);
}

// Write StringId to characters without zero-terminating. The buffer at ptr must have enough space to write the entire StringId,
// which could be eight characters. However, the caller may know the exact (or max length) in advance and act accordingly. The
// actual number of bytes written is returned.
FN_EXTERN size_t strIdToZN(StringId strId, char *const buffer);

// Convert StringId to zero-terminated string. See strIdToZN() for buffer sizing and return value.
FN_EXTERN size_t strIdToZ(StringId strId, char *const buffer);

// Get the sequence for a StringId (or error if none)
FN_EXTERN unsigned int strIdSeq(StringId strId);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
FN_EXTERN size_t strIdToLog(StringId strId, char *buffer, size_t bufferSize);

#define FUNCTION_LOG_STRING_ID_TYPE                                                                                                \
    StringId
#define FUNCTION_LOG_STRING_ID_FORMAT(value, buffer, bufferSize)                                                                   \
    strIdToLog(value, buffer, bufferSize)

#endif
