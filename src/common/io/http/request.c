/***********************************************************************************************************************************
Http Request
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
Object type
***********************************************************************************************************************************/
struct HttpRequest
{
    MemContext *memContext;                                         // Mem context

    HttpClient *httpClient;                                         // HTTP client
    IoRead *rawRead;                                                // Raw read interface passed from client
    IoRead *contentRead;                                            // Read interface for response content

    unsigned int code;                                              // Response code (e.g. 200, 404)
    String *reason;                                                 // Response reason e.g. (OK, Not Found)
    HttpHeader *header;                                             // Response headers

    bool contentChunked;                                            // Is the response content chunked?
    uint64_t contentSize;                                           // Content size (ignored for chunked)
    uint64_t contentRemaining;                                      // Content remaining (per chunk if chunked)
    bool closeOnContentEof;                                         // Will server close after content is sent?
    bool contentExists;                                             // Does content exist?
    bool contentEof;                                                // Has all content been read?
    Buffer *content;                                                // Caches content once requested
};

OBJECT_DEFINE_MOVE(HTTP_REQUEST);
OBJECT_DEFINE_FREE(HTTP_REQUEST);

/**********************************************************************************************************************************/
HttpRequest *
httpRequestNew(const String *verb, const String *uri, const HttpQuery *query, const HttpHeader *header, const Buffer *body)
{
    FUNCTION_LOG_BEGIN(logLevelDebug)
        FUNCTION_LOG_PARAM(HTTP_CLIENT, client);
        FUNCTION_LOG_PARAM(IO_READ, read);
        FUNCTION_LOG_PARAM(STRING, verb);
        FUNCTION_LOG_PARAM(BOOL, contentCache);
    FUNCTION_LOG_END();

    ASSERT(client != NULL);
    ASSERT(read != NULL);
    ASSERT(verb != NULL);

    HttpRequest *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("HttpRequest")
    {
        this = memNew(sizeof(HttpRequest));

        *this = (HttpRequest)
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

        this->code = cvtZToUInt(strPtr(strSubN(status, 0, (size_t)spacePos)));

        // Read reason phrase. A missing reason phrase will be represented as an empty string.
        MEM_CONTEXT_BEGIN(this->memContext)
        {
            this->reason = strSub(status, (size_t)spacePos + 1);
        }
        MEM_CONTEXT_END();

        // Read headers
        MEM_CONTEXT_BEGIN(this->memContext)
        {
            this->header = httpHeaderNew(NULL);
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

            httpHeaderAdd(this->header, headerKey, headerValue);

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
            this->contentRead = ioReadNewP(this, .eof = httpRequestEof, .read = httpRequestRead);
            ioReadOpen(this->contentRead);
        }
        MEM_CONTEXT_END();

        // If the server notified that it would close the connection and there is no content then close the client side
        if (!this->contentExists)
        {
            httpRequestDone(this);
        }
        // Else cache content when requested or on error
        else if (contentCache || !httpRequestCodeOk(this))
        {
            httpRequestContent(this);
        }
        // Else set callback to ensure the http client is marked done
        else
            memContextCallbackSet(this->memContext, httpRequestFreeResource, this);
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_LOG_RETURN(HTTP_REQUEST, this);
}

/**********************************************************************************************************************************/
const Buffer *
httpRequestContent(HttpRequest *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(HTTP_REQUEST, this);
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
                httpRequestRead(this, this->content, true);
            }
            while (!httpRequestEof(this));

            bufResize(this->content, bufUsed(this->content));
        }
    }

    FUNCTION_TEST_RETURN(this->content);
}

/**********************************************************************************************************************************/
bool
httpRequestBusy(const HttpRequest *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(HTTP_REQUEST, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->httpClient != NULL);
}

/**********************************************************************************************************************************/
void
httpRequestDone(HttpRequest *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(HTTP_REQUEST, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    if (this->httpClient != NULL)
    {
        memContextCallbackClear(this->memContext);
        httpRequestFreeResource(this);

        // The http client and read should no longer be used because they may get assigned to a different response
        this->httpClient = NULL;
        this->rawRead = NULL;
    }

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
bool
httpRequestCodeOk(const HttpRequest *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(HTTP_REQUEST, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->code / 100 == 2);
}

/**********************************************************************************************************************************/
String *
httpRequestToLog(const HttpRequest *this)
{
    return strNewFmt(
        "{code: %u, reason: %s, header: %s, contentChunked: %s, contentSize: %" PRIu64 ", contentRemaining: %" PRIu64
            ", closeOnContentEof: %s, contentExists: %s, contentEof: %s, contentCached: %s",
        this->code, strPtr(this->reason), strPtr(httpHeaderToLog(this->header)),
        cvtBoolToConstZ(this->contentChunked), this->contentSize, this->contentRemaining, cvtBoolToConstZ(this->closeOnContentEof),
        cvtBoolToConstZ(this->contentExists), cvtBoolToConstZ(this->contentEof), cvtBoolToConstZ(this->content != NULL));
}
