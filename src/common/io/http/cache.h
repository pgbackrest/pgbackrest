/***********************************************************************************************************************************
Http Client Cache

Cache http clients and return one that is not busy on request.
***********************************************************************************************************************************/
#ifndef COMMON_IO_HTTP_CLIENT_CACHE_H
#define COMMON_IO_HTTP_CLIENT_CACHE_H

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
#define HTTP_CLIENT_CACHE_TYPE                                      HttpClientCache
#define HTTP_CLIENT_CACHE_PREFIX                                    httpClientCache

typedef struct HttpClientCache HttpClientCache;

#include "common/io/http/client.h"

/***********************************************************************************************************************************
Constructor
***********************************************************************************************************************************/
HttpClientCache *httpClientCacheNew(
    const String *host, unsigned int port, TimeMSec timeout, bool verifyPeer, const String *caFile, const String *caPath);

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Get an http client from the cache
HttpClient *httpClientCacheGet(HttpClientCache *this);

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
void httpClientCacheFree(HttpClientCache *this);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_HTTP_CLIENT_CACHE_TYPE                                                                                        \
    HttpClientCache *
#define FUNCTION_LOG_HTTP_CLIENT_CACHE_FORMAT(value, buffer, bufferSize)                                                           \
    objToLog(value, "HttpClientCache", buffer, bufferSize)

#endif
