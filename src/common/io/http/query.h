/***********************************************************************************************************************************
HTTP Query

Object to track HTTP queries and output them with proper escaping.
***********************************************************************************************************************************/
#ifndef COMMON_IO_HTTP_QUERY_H
#define COMMON_IO_HTTP_QUERY_H

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct HttpQuery HttpQuery;

#include "common/type/object.h"
#include "common/type/stringList.h"

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
typedef struct HttpQueryNewParam
{
    VAR_PARAM_HEADER;
    const StringList *redactList;                                                    // List of keys to redact values for
} HttpQueryNewParam;

#define httpQueryNewP(...)                                                                                                         \
    httpQueryNew((HttpQueryNewParam){VAR_PARAM_INIT, __VA_ARGS__})

HttpQuery *httpQueryNew(HttpQueryNewParam param);

// New from encoded query string
HttpQuery *httpQueryNewStr(const String *query);

// Duplicate
typedef struct HttpQueryDupParam
{
    VAR_PARAM_HEADER;
    const StringList *redactList;                                                    // List of keys to redact values for
} HttpQueryDupParam;

#define httpQueryDupP(query, ...)                                                                                                  \
    httpQueryDup(query, (HttpQueryDupParam){VAR_PARAM_INIT, __VA_ARGS__})

HttpQuery *httpQueryDup(const HttpQuery *query, HttpQueryDupParam param);

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Add a query item
HttpQuery *httpQueryAdd(HttpQuery *this, const String *key, const String *value);

// Get a value using the key
const String *httpQueryGet(const HttpQuery *this, const String *key);

// Get list of keys
StringList *httpQueryList(const HttpQuery *this);

// Merge the contents of another query into this one
HttpQuery *httpQueryMerge(HttpQuery *this, const HttpQuery *query);

// Move to a new parent mem context
__attribute__((always_inline)) static inline HttpQuery *
httpQueryMove(HttpQuery *this, MemContext *parentNew)
{
    return objMove(this, parentNew);
}

// Put a query item
HttpQuery *httpQueryPut(HttpQuery *this, const String *header, const String *value);

// Should the query key be redacted when logging?
bool httpQueryRedact(const HttpQuery *this, const String *key);

// Render the query for inclusion in an HTTP request
typedef struct HttpQueryRenderParam
{
    VAR_PARAM_HEADER;
    bool redact;                                                    // Redact user-visible query string
} HttpQueryRenderParam;

#define httpQueryRenderP(this, ...)                                                                                                \
    httpQueryRender(this, (HttpQueryRenderParam){VAR_PARAM_INIT, __VA_ARGS__})

String *httpQueryRender(const HttpQuery *this, HttpQueryRenderParam param);

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
__attribute__((always_inline)) static inline void
httpQueryFree(HttpQuery *this)
{
    objFree(this);
}

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
String *httpQueryToLog(const HttpQuery *this);

#define FUNCTION_LOG_HTTP_QUERY_TYPE                                                                                               \
    HttpQuery *
#define FUNCTION_LOG_HTTP_QUERY_FORMAT(value, buffer, bufferSize)                                                                  \
    FUNCTION_LOG_STRING_OBJECT_FORMAT(value, httpQueryToLog, buffer, bufferSize)

#endif
