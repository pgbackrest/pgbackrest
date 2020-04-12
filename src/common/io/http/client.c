/***********************************************************************************************************************************
Http Client
***********************************************************************************************************************************/
#include "build.auto.h"

#include "common/debug.h"
#include "common/io/http/client.h"
#include "common/io/http/common.h"
#include "common/io/io.h"
#include "common/io/read.intern.h"
#include "common/io/tls/client.h"
#include "common/log.h"
#include "common/type/object.h"
#include "common/wait.h"

/***********************************************************************************************************************************
Http constants
***********************************************************************************************************************************/
#define HTTP_VERSION                                                "HTTP/1.1"
    STRING_STATIC(HTTP_VERSION_STR,                                 HTTP_VERSION);

STRING_EXTERN(HTTP_VERB_DELETE_STR,                                 HTTP_VERB_DELETE);
STRING_EXTERN(HTTP_VERB_GET_STR,                                    HTTP_VERB_GET);
STRING_EXTERN(HTTP_VERB_HEAD_STR,                                   HTTP_VERB_HEAD);
STRING_EXTERN(HTTP_VERB_POST_STR,                                   HTTP_VERB_POST);
STRING_EXTERN(HTTP_VERB_PUT_STR,                                    HTTP_VERB_PUT);

#define HTTP_HEADER_CONNECTION                                      "connection"
    STRING_STATIC(HTTP_HEADER_CONNECTION_STR,                       HTTP_HEADER_CONNECTION);
STRING_EXTERN(HTTP_HEADER_CONTENT_LENGTH_STR,                       HTTP_HEADER_CONTENT_LENGTH);
STRING_EXTERN(HTTP_HEADER_CONTENT_MD5_STR,                          HTTP_HEADER_CONTENT_MD5);
#define HTTP_HEADER_TRANSFER_ENCODING                               "transfer-encoding"
    STRING_STATIC(HTTP_HEADER_TRANSFER_ENCODING_STR,                HTTP_HEADER_TRANSFER_ENCODING);
STRING_EXTERN(HTTP_HEADER_ETAG_STR,                                 HTTP_HEADER_ETAG);
STRING_EXTERN(HTTP_HEADER_LAST_MODIFIED_STR,                        HTTP_HEADER_LAST_MODIFIED);

#define HTTP_VALUE_CONNECTION_CLOSE                                 "close"
    STRING_STATIC(HTTP_VALUE_CONNECTION_CLOSE_STR,                  HTTP_VALUE_CONNECTION_CLOSE);
#define HTTP_VALUE_TRANSFER_ENCODING_CHUNKED                        "chunked"
    STRING_STATIC(HTTP_VALUE_TRANSFER_ENCODING_CHUNKED_STR,         HTTP_VALUE_TRANSFER_ENCODING_CHUNKED);

// 5xx errors that should always be retried
#define HTTP_RESPONSE_CODE_RETRY_CLASS                              5

/***********************************************************************************************************************************
Statistics
***********************************************************************************************************************************/
static HttpClientStat httpClientStatLocal;

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct HttpClient
{
    MemContext *memContext;                                         // Mem context
    TimeMSec timeout;                                               // Request timeout

    TlsClient *tls;                                                 // Tls client
    IoRead *ioRead;                                                 // Read io interface

    unsigned int responseCode;                                      // Response code (e.g. 200, 404)
    String *responseMessage;                                        // Response message e.g. (OK, Not Found)
    HttpHeader *responseHeader;                                     // Response headers

    bool contentChunked;                                            // Is the response content chunked?
    uint64_t contentSize;                                           // Content size (ignored for chunked)
    uint64_t contentRemaining;                                      // Content remaining (per chunk if chunked)
    bool closeOnContentEof;                                         // Will server close after content is sent?
    bool contentEof;                                                // Has all content been read?
};

OBJECT_DEFINE_FREE(HTTP_CLIENT);

/***********************************************************************************************************************************
Read content
***********************************************************************************************************************************/
static size_t
httpClientRead(THIS_VOID, Buffer *buffer, bool block)
{
    THIS(HttpClient);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(HTTP_CLIENT, this);
        FUNCTION_LOG_PARAM(BUFFER, buffer);
        FUNCTION_LOG_PARAM(BOOL, block);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(buffer != NULL);
    ASSERT(!bufFull(buffer));

    // Read if EOF has not been reached
    size_t actualBytes = 0;

    if (!this->contentEof)
    {
        // If close was requested and no content specified then the server may send content up until the eof
        if (this->closeOnContentEof && !this->contentChunked && this->contentSize == 0)
        {
            ioRead(tlsClientIoRead(this->tls), buffer);
            this->contentEof = ioReadEof(tlsClientIoRead(this->tls));
        }
        // Else read using specified encoding or size
        else
        {
            do
            {
                // If chunked content and no content remaining
                if (this->contentChunked && this->contentRemaining == 0)
                {
                    // Read length of next chunk
                    MEM_CONTEXT_TEMP_BEGIN()
                    {
                        this->contentRemaining = cvtZToUInt64Base(strPtr(strTrim(ioReadLine(tlsClientIoRead(this->tls)))), 16);
                    }
                    MEM_CONTEXT_TEMP_END();

                    // If content remaining is still zero then eof
                    if (this->contentRemaining == 0)
                        this->contentEof = true;
                }

                // Read if there is content remaining
                if (this->contentRemaining > 0)
                {
                    // If the buffer is larger than the content that needs to be read then limit the buffer size so the read won't
                    // block or read too far.  Casting to size_t is safe on 32-bit because we know the max buffer size is defined as
                    // less than 2^32 so content remaining can't be more than that.
                    if (bufRemains(buffer) > this->contentRemaining)
                        bufLimitSet(buffer, bufSize(buffer) - (bufRemains(buffer) - (size_t)this->contentRemaining));

                    actualBytes = bufRemains(buffer);
                    this->contentRemaining -= ioRead(tlsClientIoRead(this->tls), buffer);

                    // Error if EOF but content read is not complete
                    if (ioReadEof(tlsClientIoRead(this->tls)))
                        THROW(FileReadError, "unexpected EOF reading HTTP content");

                    // Clear limit (this works even if the limit was not set and it is easier than checking)
                    bufLimitClear(buffer);
                }

                // If no content remaining
                if (this->contentRemaining == 0)
                {
                    // If chunked then consume the blank line that follows every chunk.  There might be more chunk data so loop back
                    // around to check.
                    if (this->contentChunked)
                    {
                        ioReadLine(tlsClientIoRead(this->tls));
                    }
                    // If total content size was provided then this is eof
                    else
                        this->contentEof = true;
                }
            }
            while (!bufFull(buffer) && !this->contentEof);
        }

        // If the server notified that it would close the connection after sending content then close the client side
        if (this->contentEof && this->closeOnContentEof)
            tlsClientClose(this->tls);
    }

    FUNCTION_LOG_RETURN(SIZE, (size_t)actualBytes);
}

/***********************************************************************************************************************************
Has all content been read?
***********************************************************************************************************************************/
static bool
httpClientEof(THIS_VOID)
{
    THIS(HttpClient);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(HTTP_CLIENT, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    FUNCTION_LOG_RETURN(BOOL, this->contentEof);
}

/**********************************************************************************************************************************/
HttpClient *
httpClientNew(
    const String *host, unsigned int port, TimeMSec timeout, bool verifyPeer, const String *caFile, const String *caPath)
{
    FUNCTION_LOG_BEGIN(logLevelDebug)
        FUNCTION_LOG_PARAM(STRING, host);
        FUNCTION_LOG_PARAM(UINT, port);
        FUNCTION_LOG_PARAM(TIME_MSEC, timeout);
        FUNCTION_LOG_PARAM(BOOL, verifyPeer);
        FUNCTION_LOG_PARAM(STRING, caFile);
        FUNCTION_LOG_PARAM(STRING, caPath);
    FUNCTION_LOG_END();

    ASSERT(host != NULL);

    HttpClient *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("HttpClient")
    {
        this = memNew(sizeof(HttpClient));

        *this = (HttpClient)
        {
            .memContext = MEM_CONTEXT_NEW(),
            .timeout = timeout,
            .tls = tlsClientNew(sckClientNew(host, port, timeout), timeout, verifyPeer, caFile, caPath),
        };

        httpClientStatLocal.object++;
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_LOG_RETURN(HTTP_CLIENT, this);
}

/**********************************************************************************************************************************/
Buffer *
httpClientRequest(
    HttpClient *this, const String *verb, const String *uri, const HttpQuery *query, const HttpHeader *requestHeader,
    const Buffer *body, bool returnContent)
{
    FUNCTION_LOG_BEGIN(logLevelDebug)
        FUNCTION_LOG_PARAM(HTTP_CLIENT, this);
        FUNCTION_LOG_PARAM(STRING, verb);
        FUNCTION_LOG_PARAM(STRING, uri);
        FUNCTION_LOG_PARAM(HTTP_QUERY, query);
        FUNCTION_LOG_PARAM(HTTP_HEADER, requestHeader);
        FUNCTION_LOG_PARAM(BUFFER, body);
        FUNCTION_LOG_PARAM(BOOL, returnContent);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(verb != NULL);
    ASSERT(uri != NULL);

    // Buffer for returned content
    Buffer *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        bool complete = false;
        bool retry;
        Wait *wait = waitNew(this->timeout);

        do
        {
            // Assume there will be no retry
            retry = false;

            // Free the read interface
            httpClientDone(this);

            // Free response status left over from the last request
            httpHeaderFree(this->responseHeader);
            this->responseHeader = NULL;
            strFree(this->responseMessage);
            this->responseMessage = NULL;

            // Reset all content info
            this->contentChunked = false;
            this->contentSize = 0;
            this->contentRemaining = 0;
            this->closeOnContentEof = false;
            this->contentEof = true;

            TRY_BEGIN()
            {
                if (tlsClientOpen(this->tls))
                    httpClientStatLocal.session++;

                // Write the request
                String *queryStr = httpQueryRender(query);

                ioWriteStrLine(
                    tlsClientIoWrite(this->tls),
                    strNewFmt(
                        "%s %s%s%s " HTTP_VERSION "\r", strPtr(verb), strPtr(httpUriEncode(uri, true)), queryStr == NULL ? "" : "?",
                        queryStr == NULL ? "" : strPtr(queryStr)));

                // Write headers
                if (requestHeader != NULL)
                {
                    const StringList *headerList = httpHeaderList(requestHeader);

                    for (unsigned int headerIdx = 0; headerIdx < strLstSize(headerList); headerIdx++)
                    {
                        const String *headerKey = strLstGet(headerList, headerIdx);
                        ioWriteStrLine(
                            tlsClientIoWrite(this->tls),
                            strNewFmt("%s:%s\r", strPtr(headerKey), strPtr(httpHeaderGet(requestHeader, headerKey))));
                    }
                }

                // Write out blank line to end the headers
                ioWriteLine(tlsClientIoWrite(this->tls), CR_BUF);

                // Write out body if any
                if (body != NULL)
                    ioWrite(tlsClientIoWrite(this->tls), body);

                // Flush all writes
                ioWriteFlush(tlsClientIoWrite(this->tls));

                // Read status and make sure it starts with the correct http version
                String *status = strTrim(ioReadLine(tlsClientIoRead(this->tls)));

                if (!strBeginsWith(status, HTTP_VERSION_STR))
                    THROW_FMT(FormatError, "http version of response '%s' must be " HTTP_VERSION, strPtr(status));

                // Now read the response code and message
                status = strSub(status, sizeof(HTTP_VERSION));

                int spacePos = strChr(status, ' ');

                if (spacePos < 0)
                    THROW_FMT(FormatError, "response status '%s' must have a space", strPtr(status));

                this->responseCode = cvtZToUInt(strPtr(strTrim(strSubN(status, 0, (size_t)spacePos))));

                MEM_CONTEXT_BEGIN(this->memContext)
                {
                    this->responseMessage = strSub(status, (size_t)spacePos + 1);
                }
                MEM_CONTEXT_END();

                // Read headers
                MEM_CONTEXT_BEGIN(this->memContext)
                {
                    this->responseHeader = httpHeaderNew(NULL);
                }
                MEM_CONTEXT_END();

                do
                {
                    // Read the next header
                    String *header = strTrim(ioReadLine(tlsClientIoRead(this->tls)));

                    // If the header is empty then we have reached the end of the headers
                    if (strSize(header) == 0)
                        break;

                    // Split the header and store it
                    int colonPos = strChr(header, ':');

                    if (colonPos < 0)
                        THROW_FMT(FormatError, "header '%s' missing colon", strPtr(strTrim(header)));

                    String *headerKey = strLower(strTrim(strSubN(header, 0, (size_t)colonPos)));
                    String *headerValue = strTrim(strSub(header, (size_t)colonPos + 1));

                    httpHeaderAdd(this->responseHeader, headerKey, headerValue);

                    // Read transfer encoding (only chunked is supported)
                    if (strEq(headerKey, HTTP_HEADER_TRANSFER_ENCODING_STR))
                    {
                        // Error if transfer encoding is not chunked
                        if (!strEq(headerValue, HTTP_VALUE_TRANSFER_ENCODING_CHUNKED_STR))
                        {
                            THROW_FMT(
                                FormatError, "only '%s' is supported for '%s' header", HTTP_VALUE_TRANSFER_ENCODING_CHUNKED,
                                HTTP_HEADER_TRANSFER_ENCODING);
                        }

                        this->contentChunked = true;
                    }

                    // Read content size
                    if (strEq(headerKey, HTTP_HEADER_CONTENT_LENGTH_STR))
                    {
                        this->contentSize = cvtZToUInt64(strPtr(headerValue));
                        this->contentRemaining = this->contentSize;
                    }

                    // If the server notified of a closed connection then close the client connection after reading content.  This
                    // prevents doing a retry on the next request when using the closed connection.
                    if (strEq(headerKey, HTTP_HEADER_CONNECTION_STR) && strEq(headerValue, HTTP_VALUE_CONNECTION_CLOSE_STR))
                    {
                        this->closeOnContentEof = true;
                        httpClientStatLocal.close++;
                    }
                }
                while (1);

                // Error if transfer encoding and content length are both set
                if (this->contentChunked && this->contentSize > 0)
                {
                    THROW_FMT(
                        FormatError,  "'%s' and '%s' headers are both set", HTTP_HEADER_TRANSFER_ENCODING,
                        HTTP_HEADER_CONTENT_LENGTH);
                }

                // Was content returned in the response?  HEAD will report content but not actually return any.
                bool contentExists =
                    (this->contentChunked || this->contentSize > 0 || this->closeOnContentEof) && !strEq(verb, HTTP_VERB_HEAD_STR);
                this->contentEof = !contentExists;

                // If all content should be returned from this function then read the buffer.  Also read the response if there has
                // been an error.
                if (returnContent || !httpClientResponseCodeOk(this))
                {
                    if (contentExists)
                    {
                        result = bufNew(0);

                        do
                        {
                            bufResize(result, bufSize(result) + ioBufferSize());
                            httpClientRead(this, result, true);
                        }
                        while (!httpClientEof(this));
                    }
                }
                // Else create an io object, even if there is no content.  This makes the logic for readers easier -- they can just
                // check eof rather than also checking if the io object exists.
                else
                {
                    MEM_CONTEXT_BEGIN(this->memContext)
                    {
                        this->ioRead = ioReadNewP(this, .eof = httpClientEof, .read = httpClientRead);
                        ioReadOpen(this->ioRead);
                    }
                    MEM_CONTEXT_END();
                }

                // If the server notified that it would close the connection and there is no content then close the client side
                if (this->closeOnContentEof && !contentExists)
                    tlsClientClose(this->tls);

                // Retry when response code is 5xx.  These errors generally represent a server error for a request that looks valid.
                // There are a few errors that might be permanently fatal but they are rare and it seems best not to try and pick
                // and choose errors in this class to retry.
                if (httpClientResponseCode(this) / 100 == HTTP_RESPONSE_CODE_RETRY_CLASS)
                    THROW_FMT(ServiceError, "[%u] %s", httpClientResponseCode(this), strPtr(httpClientResponseMessage(this)));

                // Request was successful
                complete = true;
            }
            CATCH_ANY()
            {
                // Retry if wait time has not expired
                if (waitMore(wait))
                {
                    LOG_DEBUG_FMT("retry %s: %s", errorTypeName(errorType()), errorMessage());
                    retry = true;

                    httpClientStatLocal.retry++;
                }

                tlsClientClose(this->tls);
            }
            TRY_END();
        }
        while (!complete && retry);

        if (!complete)
            RETHROW();

        // Move the result buffer (if any) to the parent context
        bufMove(result, memContextPrior());

        httpClientStatLocal.request++;
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(BUFFER, result);
}

/**********************************************************************************************************************************/
String *
httpClientStatStr(void)
{
    FUNCTION_TEST_VOID();

    String *result = NULL;

    if (httpClientStatLocal.object > 0)
    {
        result = strNewFmt(
            "http statistics: objects %" PRIu64 ", sessions %" PRIu64 ", requests %" PRIu64 ", retries %" PRIu64
                ", closes %" PRIu64,
            httpClientStatLocal.object, httpClientStatLocal.session, httpClientStatLocal.request, httpClientStatLocal.retry,
            httpClientStatLocal.close);
    }

    FUNCTION_TEST_RETURN(result);
}

/**********************************************************************************************************************************/
void
httpClientDone(HttpClient *this)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(HTTP_CLIENT, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    if (this->ioRead != NULL)
    {
        // If it looks like we were in the middle of a response then close the TLS session so we can start clean next time
        if (!this->contentEof)
            tlsClientClose(this->tls);

        ioReadFree(this->ioRead);
        this->ioRead = NULL;
    }

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
bool
httpClientBusy(const HttpClient *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(HTTP_CLIENT, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->ioRead);
}

/**********************************************************************************************************************************/
IoRead *
httpClientIoRead(const HttpClient *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(HTTP_CLIENT, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->ioRead);
}

/**********************************************************************************************************************************/
unsigned int
httpClientResponseCode(const HttpClient *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(HTTP_CLIENT, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->responseCode);
}

/**********************************************************************************************************************************/
bool
httpClientResponseCodeOk(const HttpClient *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(HTTP_CLIENT, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->responseCode / 100 == 2);
}

/**********************************************************************************************************************************/
const HttpHeader *
httpClientResponseHeader(const HttpClient *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(HTTP_CLIENT, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->responseHeader);
}

/**********************************************************************************************************************************/
const String *
httpClientResponseMessage(const HttpClient *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(HTTP_CLIENT, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->responseMessage);
}
