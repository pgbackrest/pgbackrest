/***********************************************************************************************************************************
Http Client
***********************************************************************************************************************************/
#include "common/assert.h"
#include "common/debug.h"
#include "common/io/http/client.h"
#include "common/io/http/common.h"
#include "common/io/io.h"
#include "common/io/read.intern.h"
#include "common/io/tls/client.h"
#include "common/log.h"
#include "common/wait.h"

/***********************************************************************************************************************************
Http constants
***********************************************************************************************************************************/
#define HTTP_VERSION                                                "HTTP/1.1"
    STRING_STATIC(HTTP_VERSION_STR,                                 HTTP_VERSION);

STRING_EXTERN(HTTP_VERB_GET_STR,                                    HTTP_VERB_GET);

#define HTTP_HEADER_CONNECTION                                      "connection"
    STRING_STATIC(HTTP_HEADER_CONNECTION_STR,                       HTTP_HEADER_CONNECTION);
STRING_EXTERN(HTTP_HEADER_CONTENT_LENGTH_STR,                       HTTP_HEADER_CONTENT_LENGTH);
#define HTTP_HEADER_TRANSFER_ENCODING                               "transfer-encoding"
    STRING_STATIC(HTTP_HEADER_TRANSFER_ENCODING_STR,                HTTP_HEADER_TRANSFER_ENCODING);

#define HTTP_VALUE_CONNECTION_CLOSE                                 "close"
    STRING_STATIC(HTTP_VALUE_CONNECTION_CLOSE_STR,                  HTTP_VALUE_CONNECTION_CLOSE);
#define HTTP_VALUE_TRANSFER_ENCODING_CHUNKED                        "chunked"
    STRING_STATIC(HTTP_VALUE_TRANSFER_ENCODING_CHUNKED_STR,         HTTP_VALUE_TRANSFER_ENCODING_CHUNKED);

// 5xx errors that should always be retried
#define HTTP_RESPONSE_CODE_RETRY_CLASS                              5

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

    bool contentChunked;                                            // Is the reponse content chunked?
    uint64_t contentSize;                                           // Content size (ignored for chunked)
    uint64_t contentRemaining;                                      // Content remaining (per chunk if chunked)
    bool closeOnContentEof;                                         // Will server close after content is sent?
    bool contentEof;                                                // Has all content been read?
};

/***********************************************************************************************************************************
Read content
***********************************************************************************************************************************/
static size_t
httpClientRead(HttpClient *this, Buffer *buffer)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(HTTP_CLIENT, this);
        FUNCTION_DEBUG_PARAM(BUFFER, buffer);

        FUNCTION_TEST_ASSERT(this != NULL);
        FUNCTION_TEST_ASSERT(buffer != NULL);
        FUNCTION_TEST_ASSERT(!bufFull(buffer));
    FUNCTION_DEBUG_END();

    // Read if EOF has not been reached
    size_t actualBytes = 0;

    if (!this->contentEof)
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
                // If the buffer is larger than the content that needs to read then limit the buffer size so the read won't block
                // or read too far.  Casting to size_t is safe on 32-bit because we know the max buffer size is defined as less than
                // 2^32 so content remaining can't be more than that.
                if (bufRemains(buffer) > this->contentRemaining)
                    bufLimitSet(buffer, bufSize(buffer) - (bufRemains(buffer) - (size_t)this->contentRemaining));

                this->contentRemaining -= ioRead(tlsClientIoRead(this->tls), buffer);

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

        // If the server notified that it would close the connection after sending content then close the client side
        if (this->contentEof && this->closeOnContentEof)
            tlsClientClose(this->tls);
    }

    FUNCTION_DEBUG_RESULT(SIZE, (size_t)actualBytes);
}

/***********************************************************************************************************************************
Has all content been read?
***********************************************************************************************************************************/
static bool
httpClientEof(const HttpClient *this)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(HTTP_CLIENT, this);

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_DEBUG_END();

    FUNCTION_DEBUG_RESULT(BOOL, this->contentEof);
}

/***********************************************************************************************************************************
New object
***********************************************************************************************************************************/
HttpClient *
httpClientNew(
    const String *host, unsigned int port, TimeMSec timeout, bool verifyPeer, const String *caFile, const String *caPath)
{
    FUNCTION_DEBUG_BEGIN(logLevelDebug)
        FUNCTION_DEBUG_PARAM(STRING, host);
        FUNCTION_DEBUG_PARAM(UINT, port);
        FUNCTION_DEBUG_PARAM(TIME_MSEC, timeout);
        FUNCTION_DEBUG_PARAM(BOOL, verifyPeer);
        FUNCTION_DEBUG_PARAM(STRING, caFile);
        FUNCTION_DEBUG_PARAM(STRING, caPath);

        FUNCTION_TEST_ASSERT(host != NULL);
    FUNCTION_DEBUG_END();

    HttpClient *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("HttpClient")
    {
        // Allocate state and set context
        this = memNew(sizeof(HttpClient));
        this->memContext = MEM_CONTEXT_NEW();

        this->timeout = timeout;
        this->tls = tlsClientNew(host, port, timeout, verifyPeer, caFile, caPath);
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_DEBUG_RESULT(HTTP_CLIENT, this);
}

/***********************************************************************************************************************************
Perform a request
***********************************************************************************************************************************/
Buffer *
httpClientRequest(
    HttpClient *this, const String *verb, const String *uri, const HttpQuery *query, const HttpHeader *requestHeader,
    bool returnContent)
{
    FUNCTION_DEBUG_BEGIN(logLevelDebug)
        FUNCTION_DEBUG_PARAM(HTTP_CLIENT, this);
        FUNCTION_DEBUG_PARAM(STRING, verb);
        FUNCTION_DEBUG_PARAM(STRING, uri);
        FUNCTION_DEBUG_PARAM(HTTP_QUERY, query);
        FUNCTION_DEBUG_PARAM(HTTP_HEADER, requestHeader);

        FUNCTION_TEST_ASSERT(this != NULL);
        FUNCTION_TEST_ASSERT(verb != NULL);
        FUNCTION_TEST_ASSERT(uri != NULL);
    FUNCTION_DEBUG_END();

    // Buffer for returned content
    Buffer *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        bool complete = false;
        bool retry;
        Wait *wait = this->timeout > 0 ? waitNew(this->timeout) : NULL;

        do
        {
            // Assume there will be no retry
            retry = false;

            // Free the read interface
            ioReadFree(this->ioRead);
            this->ioRead = NULL;

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
                tlsClientOpen(this->tls);

                // Write the request
                String *queryStr = httpQueryRender(query);

                ioWriteLine(
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
                        ioWriteLine(
                            tlsClientIoWrite(this->tls),
                            strNewFmt("%s:%s\r", strPtr(headerKey), strPtr(httpHeaderGet(requestHeader, headerKey))));
                    }
                }

                // Write out blank line and close the write so it flushes
                ioWriteLine(tlsClientIoWrite(this->tls), CR_STR);
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
                        this->closeOnContentEof = true;
                }
                while (1);

                // Error if transfer encoding and content length are both set
                if (this->contentChunked && this->contentSize > 0)
                {
                    THROW_FMT(
                        FormatError,  "'%s' and '%s' headers are both set", HTTP_HEADER_TRANSFER_ENCODING,
                        HTTP_HEADER_CONTENT_LENGTH);
                }

                // If content chunked or content length > 0 then there is content to read
                if (this->contentChunked || this->contentSize > 0)
                {
                    this->contentEof = false;

                    // If all content should be returned from this function then read the buffer.  Also read the reponse if there
                    // has been an error.
                    if (returnContent || httpClientResponseCode(this) != 200)
                    {
                        result = bufNew(0);

                        do
                        {
                            bufResize(result, bufSize(result) + ioBufferSize());
                            httpClientRead(this, result);
                        }
                        while (!httpClientEof(this));
                    }
                    // Else create the read interface
                    else
                    {
                        MEM_CONTEXT_BEGIN(this->memContext)
                        {
                            this->ioRead = ioReadNewP(
                                this, .eof = (IoReadInterfaceEof)httpClientEof, .read = (IoReadInterfaceRead)httpClientRead);
                            ioReadOpen(this->ioRead);
                        }
                        MEM_CONTEXT_END();
                    }
                }
                // If the server notified that it would close the connection after sending content then close the client side
                else if (this->closeOnContentEof)
                    tlsClientClose(this->tls);

                // Retry when reponse code is 5xx.  These errors generally represent a server error for a request that looks valid.
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
                if (wait != NULL && waitMore(wait))
                {
                    LOG_DEBUG("retry %s: %s", errorTypeName(errorType()), errorMessage());
                    retry = true;
                }

                tlsClientClose(this->tls);
            }
            TRY_END();
        }
        while (!complete && retry);

        if (!complete)
            RETHROW();

        // Move the result buffer (if any) to the parent context
        bufMove(result, MEM_CONTEXT_OLD());
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_DEBUG_RESULT(BUFFER, result);
}

/***********************************************************************************************************************************
Get read interface
***********************************************************************************************************************************/
IoRead *
httpClientIoRead(const HttpClient *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(HTTP_CLIENT, this);

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RESULT(IO_READ, this->ioRead);
}

/***********************************************************************************************************************************
Get the response code
***********************************************************************************************************************************/
unsigned int
httpClientResponseCode(const HttpClient *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(HTTP_CLIENT, this);

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RESULT(UINT, this->responseCode);
}

/***********************************************************************************************************************************
Get the response headers
***********************************************************************************************************************************/
const HttpHeader *
httpClientReponseHeader(const HttpClient *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(HTTP_CLIENT, this);

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RESULT(HTTP_HEADER, this->responseHeader);
}

/***********************************************************************************************************************************
Get the response message
***********************************************************************************************************************************/
const String *
httpClientResponseMessage(const HttpClient *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(HTTP_CLIENT, this);

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RESULT(STRING, this->responseMessage);
}

/***********************************************************************************************************************************
Free the object
***********************************************************************************************************************************/
void
httpClientFree(HttpClient *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(HTTP_CLIENT, this);
    FUNCTION_TEST_END();

    if (this != NULL)
        memContextFree(this->memContext);

    FUNCTION_TEST_RESULT_VOID();
}
