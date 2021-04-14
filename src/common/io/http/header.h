/***********************************************************************************************************************************
HTTP Header

Object to track HTTP headers.  Headers can be marked as redacted so they are not logged.
***********************************************************************************************************************************/
#ifndef COMMON_IO_HTTP_HEADER_H
#define COMMON_IO_HTTP_HEADER_H

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct HttpHeader HttpHeader;

#include "common/type/object.h"
#include "common/type/stringList.h"

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
HttpHeader *httpHeaderNew(const StringList *redactList);
HttpHeader *httpHeaderDup(const HttpHeader *header, const StringList *redactList);

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Add a header
HttpHeader *httpHeaderAdd(HttpHeader *this, const String *key, const String *value);

// Get a value using the key
const String *httpHeaderGet(const HttpHeader *this, const String *key);

// Get list of keys
StringList *httpHeaderList(const HttpHeader *this);

// Move to a new parent mem context
__attribute__((always_inline)) static inline HttpHeader *
httpHeaderMove(HttpHeader *this, MemContext *parentNew)
{
    return objMove(this, parentNew);
}

// Put a header
HttpHeader *httpHeaderPut(HttpHeader *this, const String *header, const String *value);

// Should the header be redacted when logging?
bool httpHeaderRedact(const HttpHeader *this, const String *key);

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
__attribute__((always_inline)) static inline void
httpHeaderFree(HttpHeader *this)
{
    objFree(this);
}

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
String *httpHeaderToLog(const HttpHeader *this);

#define FUNCTION_LOG_HTTP_HEADER_TYPE                                                                                              \
    HttpHeader *
#define FUNCTION_LOG_HTTP_HEADER_FORMAT(value, buffer, bufferSize)                                                                 \
    FUNCTION_LOG_STRING_OBJECT_FORMAT(value, httpHeaderToLog, buffer, bufferSize)

#endif
