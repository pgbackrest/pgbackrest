/***********************************************************************************************************************************
HTTP URL

Parse a URL into component parts.
***********************************************************************************************************************************/
#ifndef COMMON_IO_HTTP_URL_H
#define COMMON_IO_HTTP_URL_H

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct HttpUrl HttpUrl;

#include "common/type/object.h"
#include "common/type/param.h"
#include "common/type/string.h"

/***********************************************************************************************************************************
HTTP protocol type
***********************************************************************************************************************************/
typedef enum
{
    httpProtocolTypeAny = 0,
    httpProtocolTypeHttp = 1,
    httpProtocolTypeHttps = 2,
} HttpProtocolType;

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
typedef struct HttpUrlNewParseParam
{
    VAR_PARAM_HEADER;
    HttpProtocolType type;                                          // Expected protocol type (httpProtocolTypeAny if any)
} HttpUrlNewParseParam;

#define httpUrlNewParseP(url, ...)                                                                                                 \
    httpUrlNewParse(url, (HttpUrlNewParseParam){VAR_PARAM_INIT, __VA_ARGS__})

FN_EXTERN HttpUrl *httpUrlNewParse(const String *const url, HttpUrlNewParseParam param);

/***********************************************************************************************************************************
Getters/setters
***********************************************************************************************************************************/
typedef struct HttpUrlPub
{
    const String *url;                                              // Original URL
    HttpProtocolType type;                                          // Protocol type, e.g. http
    const String *host;                                             // Host
    unsigned int port;                                              // Port
    const String *path;                                             // Path
} HttpUrlPub;

// Protocol type
FN_INLINE_ALWAYS HttpProtocolType
httpUrlProtocolType(const HttpUrl *const this)
{
    return THIS_PUB(HttpUrl)->type;
}

// Host
FN_INLINE_ALWAYS const String *
httpUrlHost(const HttpUrl *const this)
{
    return THIS_PUB(HttpUrl)->host;
}

// Path
FN_INLINE_ALWAYS const String *
httpUrlPath(const HttpUrl *const this)
{
    return THIS_PUB(HttpUrl)->path;
}

// Port
FN_INLINE_ALWAYS unsigned int
httpUrlPort(const HttpUrl *const this)
{
    return THIS_PUB(HttpUrl)->port;
}

// URL (exactly as originally passed)
FN_INLINE_ALWAYS const String *
httpUrl(const HttpUrl *const this)
{
    return THIS_PUB(HttpUrl)->url;
}

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
FN_INLINE_ALWAYS void
httpUrlFree(HttpUrl *const this)
{
    objFree(this);
}

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#ifdef DEBUG

FN_EXTERN void httpUrlToLog(const HttpUrl *this, StringStatic *debugLog);

#define FUNCTION_LOG_HTTP_URL_TYPE                                                                                               \
    HttpUrl *
#define FUNCTION_LOG_HTTP_URL_FORMAT(value, buffer, bufferSize)                                                                  \
    FUNCTION_LOG_OBJECT_FORMAT(value, httpUrlToLog, buffer, bufferSize)

#endif // DEBUG

#endif
