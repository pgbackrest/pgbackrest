/***********************************************************************************************************************************
HTTP Header

Object to track HTTP headers. Headers can be marked as redacted so they are not logged.
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
FN_EXTERN HttpHeader *httpHeaderNew(const StringList *redactList);
FN_EXTERN HttpHeader *httpHeaderDup(const HttpHeader *header, const StringList *redactList);

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Add a header
FN_EXTERN HttpHeader *httpHeaderAdd(HttpHeader *this, const String *key, const String *value);

// Get a value using the key
FN_EXTERN const String *httpHeaderGet(const HttpHeader *this, const String *key);

// Get list of keys
FN_EXTERN StringList *httpHeaderList(const HttpHeader *this);

// Move to a new parent mem context
FN_INLINE_ALWAYS HttpHeader *
httpHeaderMove(HttpHeader *const this, MemContext *const parentNew)
{
    return objMove(this, parentNew);
}

// Put a header
FN_EXTERN HttpHeader *httpHeaderPut(HttpHeader *this, const String *header, const String *value);

// Put range header when needed
FN_EXTERN HttpHeader *httpHeaderPutRange(HttpHeader *this, uint64_t offset, const Variant *limit);

// Should the header be redacted when logging?
FN_EXTERN bool httpHeaderRedact(const HttpHeader *this, const String *key);

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
FN_INLINE_ALWAYS void
httpHeaderFree(HttpHeader *const this)
{
    objFree(this);
}

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
FN_EXTERN void httpHeaderToLog(const HttpHeader *this, StringStatic *debugLog);

#define FUNCTION_LOG_HTTP_HEADER_TYPE                                                                                              \
    HttpHeader *
#define FUNCTION_LOG_HTTP_HEADER_FORMAT(value, buffer, bufferSize)                                                                 \
    FUNCTION_LOG_OBJECT_FORMAT(value, httpHeaderToLog, buffer, bufferSize)

#endif
