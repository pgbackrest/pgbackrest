/***********************************************************************************************************************************
Http Response
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
#define HTTP_HEADER_CONNECTION                                      "connection"
    STRING_STATIC(HTTP_HEADER_CONNECTION_STR,                       HTTP_HEADER_CONNECTION);
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
struct HttpResponse
{
    MemContext *memContext;                                         // Mem context

    HttpClient *httpClient;                                         // HTTP client
    IoRead *rawRead;                                                // Raw read interface passed from client
    IoRead *contentRead;                                            // Read interface for response content

    unsigned int responseCode;                                      // Response code (e.g. 200, 404)
    String *responseMessage;                                        // Response message e.g. (OK, Not Found)
    HttpHeader *responseHeader;                                     // Response headers

    bool contentChunked;                                            // Is the response content chunked?
    uint64_t contentSize;                                           // Content size (ignored for chunked)
    uint64_t contentRemaining;                                      // Content remaining (per chunk if chunked)
    bool closeOnContentEof;                                         // Will server close after content is sent?
    bool contentExists;                                             // Does content exist?
    bool contentEof;                                                // Has all content been read?
    Buffer *content;                                                // Caches content once requested
};

OBJECT_DEFINE_MOVE(HTTP_RESPONSE);
OBJECT_DEFINE_FREE(HTTP_RESPONSE);

/***********************************************************************************************************************************
Mark http client as done so it can be reused
***********************************************************************************************************************************/
OBJECT_DEFINE_FREE_RESOURCE_BEGIN(HTTP_RESPONSE, LOG, logLevelTrace)
{
    httpClientDone(this->httpClient, this->closeOnContentEof, this->closeOnContentEof);
}
OBJECT_DEFINE_FREE_RESOURCE_END(LOG);

/***********************************************************************************************************************************
Read content
***********************************************************************************************************************************/
static size_t
httpResponseRead(THIS_VOID, Buffer *buffer, bool block)
{
    THIS(HttpResponse);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(HTTP_RESPONSE, this);
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
            ioRead(this->rawRead, buffer);
            this->contentEof = ioReadEof(this->rawRead);
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
                        this->contentRemaining = cvtZToUInt64Base(
                            strPtr(strTrim(ioReadLine(this->rawRead))), 16);
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
                    this->contentRemaining -= ioRead(this->rawRead, buffer);

                    // Error if EOF but content read is not complete
                    if (ioReadEof(this->rawRead))
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
                        ioReadLine(this->rawRead);
                    }
                    // If total content size was provided then this is eof
                    else
                        this->contentEof = true;
                }
            }
            while (!bufFull(buffer) && !this->contentEof);
        }

        // If all content has been read
        if (this->contentEof)
            httpResponseDone(this);
    }

    FUNCTION_LOG_RETURN(SIZE, (size_t)actualBytes);
}

/***********************************************************************************************************************************
Has all content been read?
***********************************************************************************************************************************/
static bool
httpResponseEof(THIS_VOID)
{
    THIS(HttpResponse);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(HTTP_RESPONSE, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    FUNCTION_LOG_RETURN(BOOL, this->contentEof);
}

/**********************************************************************************************************************************/
HttpResponse *
httpResponseNew(HttpClient *client, IoRead *read, const String *verb)
{
    FUNCTION_LOG_BEGIN(logLevelDebug)
        FUNCTION_LOG_PARAM(HTTP_CLIENT, client);
        FUNCTION_LOG_PARAM(IO_READ, read);
        FUNCTION_LOG_PARAM(STRING, verb);
    FUNCTION_LOG_END();

    ASSERT(client != NULL);
    ASSERT(read != NULL);
    ASSERT(verb != NULL);

    HttpResponse *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("HttpResponse")
    {
        this = memNew(sizeof(HttpResponse));

        *this = (HttpResponse)
        {
            .memContext = MEM_CONTEXT_NEW(),
            .httpClient = client,
            .rawRead = read,
        };

        // Read status
        String *status = ioReadLine(this->rawRead);

        // Check status ends with a CR and remove it to make error formatting easier and more accurate
        if (!strEndsWith(status, CR_STR))
            THROW_FMT(FormatError, "http response status '%s' should be CR-terminated", strPtr(status));

        status = strSubN(status, 0, strSize(status) - 1);

        // Check status is at least the minimum required length to avoid harder to interpret errors later on
        if (strSize(status) < sizeof(HTTP_VERSION) + 4)
            THROW_FMT(FormatError, "http response '%s' has invalid length", strPtr(strTrim(status)));

        // Check status starts with the correct http version
         if (!strBeginsWith(status, HTTP_VERSION_STR))
            THROW_FMT(FormatError, "http version of response '%s' must be " HTTP_VERSION, strPtr(status));

        // Read status code
        status = strSub(status, sizeof(HTTP_VERSION));

        int spacePos = strChr(status, ' ');

        if (spacePos != 3)
            THROW_FMT(FormatError, "response status '%s' must have a space after the status code", strPtr(status));

        this->responseCode = cvtZToUInt(strPtr(strSubN(status, 0, (size_t)spacePos)));

        // Read reason phrase. A missing reason phrase will be represented as an empty string.
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
            String *header = strTrim(ioReadLine(this->rawRead));

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

        // Was content returned in the response?  HEAD will report content but not actually return any.
        this->contentExists =
            (this->contentChunked || this->contentSize > 0 || this->closeOnContentEof) && !strEq(verb, HTTP_VERB_HEAD_STR);
        this->contentEof = !this->contentExists;

        // Create an io object, even if there is no content.  This makes the logic for readers easier -- they can just check eof
        // rather than also checking if the io object exists.
        MEM_CONTEXT_BEGIN(this->memContext)
        {
            this->contentRead = ioReadNewP(this, .eof = httpResponseEof, .read = httpResponseRead);
            ioReadOpen(this->contentRead);
        }
        MEM_CONTEXT_END();

        // Retry when response code is 5xx.  These errors generally represent a server error for a request that looks valid.
        // There are a few errors that might be permanently fatal but they are rare and it seems best not to try and pick
        // and choose errors in this class to retry.
        if (httpResponseCode(this) / 100 == HTTP_RESPONSE_CODE_RETRY_CLASS)
            THROW_FMT(ServiceError, "[%u] %s", httpResponseCode(this), strPtr(httpResponseMessage(this)));

        // If the server notified that it would close the connection and there is no content then close the client side
        if (!this->contentExists)
        {
            httpResponseDone(this);
        }
        // Else set callback to ensure the http client is marked done
        else
            memContextCallbackSet(this->memContext, httpResponseFreeResource, this);
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_LOG_RETURN(HTTP_RESPONSE, this);
}

/**********************************************************************************************************************************/
const Buffer *
httpResponseContent(HttpResponse *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(HTTP_RESPONSE, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    if (this->content == NULL)
    {
        this->content = bufNew(0);

        if (this->contentExists)
        {
            do
            {
                bufResize(this->content, bufSize(this->content) + ioBufferSize());
                httpResponseRead(this, this->content, true);
            }
            while (!httpResponseEof(this));

            bufResize(this->content, bufUsed(this->content));
        }
    }

    FUNCTION_TEST_RETURN(this->content);
}

/**********************************************************************************************************************************/
void
httpResponseDone(HttpResponse *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(HTTP_RESPONSE, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    if (this->httpClient != NULL)
    {
        memContextCallbackClear(this->memContext);
        httpResponseFreeResource(this);

        // The http client and read should no longer be used because they may get assigned to a different response
        this->httpClient = NULL;
        this->rawRead = NULL;
    }

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
IoRead *
httpResponseIoRead(HttpResponse *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(HTTP_RESPONSE, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->contentRead);
}

/**********************************************************************************************************************************/
unsigned int
httpResponseCode(const HttpResponse *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(HTTP_RESPONSE, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->responseCode);
}

/**********************************************************************************************************************************/
bool
httpResponseCodeOk(const HttpResponse *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(HTTP_RESPONSE, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->responseCode / 100 == 2);
}

/**********************************************************************************************************************************/
const HttpHeader *
httpResponseHeader(const HttpResponse *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(HTTP_RESPONSE, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->responseHeader);
}

/**********************************************************************************************************************************/
const String *
httpResponseMessage(const HttpResponse *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(HTTP_RESPONSE, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->responseMessage);
}
