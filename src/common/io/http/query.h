/***********************************************************************************************************************************
Http Query

Object to track HTTP queries and output them with proper escaping.
***********************************************************************************************************************************/
#ifndef COMMON_IO_HTTP_QUERY_H
#define COMMON_IO_HTTP_QUERY_H

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
#define HTTP_QUERY_TYPE                                             HttpQuery
#define HTTP_QUERY_PREFIX                                           httpQuery

typedef struct HttpQuery HttpQuery;

#include "common/type/stringList.h"

/***********************************************************************************************************************************
Constructor
***********************************************************************************************************************************/
HttpQuery *httpQueryNew(void);

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
HttpQuery *httpQueryAdd(HttpQuery *this, const String *key, const String *value);
const String *httpQueryGet(const HttpQuery *this, const String *key);
StringList *httpQueryList(const HttpQuery *this);
HttpQuery *httpQueryMove(HttpQuery *this, MemContext *parentNew);
HttpQuery *httpQueryPut(HttpQuery *this, const String *header, const String *value);
String *httpQueryRender(const HttpQuery *this);

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
void httpQueryFree(HttpQuery *this);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
String *httpQueryToLog(const HttpQuery *this);

#define FUNCTION_LOG_HTTP_QUERY_TYPE                                                                                               \
    HttpQuery *
#define FUNCTION_LOG_HTTP_QUERY_FORMAT(value, buffer, bufferSize)                                                                  \
    FUNCTION_LOG_STRING_OBJECT_FORMAT(value, httpQueryToLog, buffer, bufferSize)

#endif
