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
STRING_EXTERN(HTTP_VERSION_STR,                                     HTTP_VERSION);

STRING_EXTERN(HTTP_VERB_DELETE_STR,                                 HTTP_VERB_DELETE);
STRING_EXTERN(HTTP_VERB_GET_STR,                                    HTTP_VERB_GET);
STRING_EXTERN(HTTP_VERB_HEAD_STR,                                   HTTP_VERB_HEAD);
STRING_EXTERN(HTTP_VERB_POST_STR,                                   HTTP_VERB_POST);
STRING_EXTERN(HTTP_VERB_PUT_STR,                                    HTTP_VERB_PUT);

STRING_EXTERN(HTTP_HEADER_AUTHORIZATION_STR,                        HTTP_HEADER_AUTHORIZATION);
STRING_EXTERN(HTTP_HEADER_CONTENT_LENGTH_STR,                       HTTP_HEADER_CONTENT_LENGTH);
STRING_EXTERN(HTTP_HEADER_CONTENT_MD5_STR,                          HTTP_HEADER_CONTENT_MD5);
STRING_EXTERN(HTTP_HEADER_ETAG_STR,                                 HTTP_HEADER_ETAG);
STRING_EXTERN(HTTP_HEADER_HOST_STR,                                 HTTP_HEADER_HOST);
STRING_EXTERN(HTTP_HEADER_LAST_MODIFIED_STR,                        HTTP_HEADER_LAST_MODIFIED);

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
    TlsClient *tlsClient;                                           // TLS client

    TlsSession *tlsSession;                                         // Current TLS session
    bool busy;                                                      // Is the HTTP client currently busy?
};

OBJECT_DEFINE_FREE(HTTP_CLIENT);

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
            .tlsClient = tlsClientNew(sckClientNew(host, port, timeout), timeout, verifyPeer, caFile, caPath),
        };

        httpClientStatLocal.object++;
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_LOG_RETURN(HTTP_CLIENT, this);
}

/**********************************************************************************************************************************/
HttpResponse *
httpClientRequest(
    HttpClient *this, const String *verb, const String *uri, const HttpQuery *query, const HttpHeader *requestHeader,
    const Buffer *body)
{
    FUNCTION_LOG_BEGIN(logLevelDebug)
        FUNCTION_LOG_PARAM(HTTP_CLIENT, this);
        FUNCTION_LOG_PARAM(STRING, verb);
        FUNCTION_LOG_PARAM(STRING, uri);
        FUNCTION_LOG_PARAM(HTTP_QUERY, query);
        FUNCTION_LOG_PARAM(HTTP_HEADER, requestHeader);
        FUNCTION_LOG_PARAM(BUFFER, body);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(verb != NULL);
    ASSERT(uri != NULL);
    ASSERT(!this->busy);

    // HTTP Response
    HttpResponse *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        bool retry;
        Wait *wait = waitNew(this->timeout);

        do
        {
            // Assume there will be no retry
            retry = false;
            //
            // // Free the read interface
            // httpClientDone(this, false, false);

            TRY_BEGIN()
            {
                // Set client busy and load request
                this->busy = true;

                if (this->tlsSession == NULL)
                {
                    MEM_CONTEXT_BEGIN(this->memContext)
                    {
                        this->tlsSession = tlsClientOpen(this->tlsClient);
                        httpClientStatLocal.session++;
                    }
                    MEM_CONTEXT_END();
                }

                // Write the request
                String *queryStr = httpQueryRender(query);

                ioWriteStrLine(
                    tlsSessionIoWrite(this->tlsSession),
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
                            tlsSessionIoWrite(this->tlsSession),
                            strNewFmt("%s:%s\r", strPtr(headerKey), strPtr(httpHeaderGet(requestHeader, headerKey))));
                    }
                }

                // Write out blank line to end the headers
                ioWriteLine(tlsSessionIoWrite(this->tlsSession), CR_BUF);

                // Write out body if any
                if (body != NULL)
                    ioWrite(tlsSessionIoWrite(this->tlsSession), body);

                // Flush all writes
                ioWriteFlush(tlsSessionIoWrite(this->tlsSession));

                MEM_CONTEXT_PRIOR_BEGIN()
                {
                    result = httpResponseNew(this, tlsSessionIoRead(this->tlsSession), verb);
                }
                MEM_CONTEXT_PRIOR_END();

                // // Read status
                // String *status = ioReadLine(tlsSessionIoRead(this->tlsSession));
                //
                // // Check status ends with a CR and remove it to make error formatting easier and more accurate
                // if (!strEndsWith(status, CR_STR))
                //     THROW_FMT(FormatError, "http response status '%s' should be CR-terminated", strPtr(status));
                //
                // status = strSubN(status, 0, strSize(status) - 1);
                //
                // // Check status is at least the minimum required length to avoid harder to interpret errors later on
                // if (strSize(status) < sizeof(HTTP_VERSION) + 4)
                //     THROW_FMT(FormatError, "http response '%s' has invalid length", strPtr(strTrim(status)));
                //
                // // Check status starts with the correct http version
                //  if (!strBeginsWith(status, HTTP_VERSION_STR))
                //     THROW_FMT(FormatError, "http version of response '%s' must be " HTTP_VERSION, strPtr(status));
                //
                // // Read status code
                // status = strSub(status, sizeof(HTTP_VERSION));
                //
                // int spacePos = strChr(status, ' ');
                //
                // if (spacePos != 3)
                //     THROW_FMT(FormatError, "response status '%s' must have a space after the status code", strPtr(status));
                //
                // this->responseCode = cvtZToUInt(strPtr(strSubN(status, 0, (size_t)spacePos)));
                //
                // // Read reason phrase. A missing reason phrase will be represented as an empty string.
                // MEM_CONTEXT_BEGIN(this->memContext)
                // {
                //     this->responseMessage = strSub(status, (size_t)spacePos + 1);
                // }
                // MEM_CONTEXT_END();
                //
                // // Read headers
                // MEM_CONTEXT_BEGIN(this->memContext)
                // {
                //     this->responseHeader = httpHeaderNew(NULL);
                // }
                // MEM_CONTEXT_END();
                //
                // do
                // {
                //     // Read the next header
                //     String *header = strTrim(ioReadLine(tlsSessionIoRead(this->tlsSession)));
                //
                //     // If the header is empty then we have reached the end of the headers
                //     if (strSize(header) == 0)
                //         break;
                //
                //     // Split the header and store it
                //     int colonPos = strChr(header, ':');
                //
                //     if (colonPos < 0)
                //         THROW_FMT(FormatError, "header '%s' missing colon", strPtr(strTrim(header)));
                //
                //     String *headerKey = strLower(strTrim(strSubN(header, 0, (size_t)colonPos)));
                //     String *headerValue = strTrim(strSub(header, (size_t)colonPos + 1));
                //
                //     httpHeaderAdd(this->responseHeader, headerKey, headerValue);
                //
                //     // Read transfer encoding (only chunked is supported)
                //     if (strEq(headerKey, HTTP_HEADER_TRANSFER_ENCODING_STR))
                //     {
                //         // Error if transfer encoding is not chunked
                //         if (!strEq(headerValue, HTTP_VALUE_TRANSFER_ENCODING_CHUNKED_STR))
                //         {
                //             THROW_FMT(
                //                 FormatError, "only '%s' is supported for '%s' header", HTTP_VALUE_TRANSFER_ENCODING_CHUNKED,
                //                 HTTP_HEADER_TRANSFER_ENCODING);
                //         }
                //
                //         this->contentChunked = true;
                //     }
                //
                //     // Read content size
                //     if (strEq(headerKey, HTTP_HEADER_CONTENT_LENGTH_STR))
                //     {
                //         this->contentSize = cvtZToUInt64(strPtr(headerValue));
                //         this->contentRemaining = this->contentSize;
                //     }
                //
                //     // If the server notified of a closed connection then close the client connection after reading content.  This
                //     // prevents doing a retry on the next request when using the closed connection.
                //     if (strEq(headerKey, HTTP_HEADER_CONNECTION_STR) && strEq(headerValue, HTTP_VALUE_CONNECTION_CLOSE_STR))
                //     {
                //         this->closeOnContentEof = true;
                //         httpClientStatLocal.close++;
                //     }
                // }
                // while (1);
                //
                // // Error if transfer encoding and content length are both set
                // if (this->contentChunked && this->contentSize > 0)
                // {
                //     THROW_FMT(
                //         FormatError,  "'%s' and '%s' headers are both set", HTTP_HEADER_TRANSFER_ENCODING,
                //         HTTP_HEADER_CONTENT_LENGTH);
                // }
                //
                // // Was content returned in the response?  HEAD will report content but not actually return any.
                // bool contentExists =
                //     (this->contentChunked || this->contentSize > 0 || this->closeOnContentEof) && !strEq(verb, HTTP_VERB_HEAD_STR);
                // this->contentEof = !contentExists;

                // // If all content should be returned from this function then read the buffer.  Also read the response if there has
                // // been an error.
                // if (returnContent || !httpClientResponseCodeOk(this))
                // {
                //     if (contentExists)
                //     {
                //         result = bufNew(0);
                //
                //         do
                //         {
                //             bufResize(result, bufSize(result) + ioBufferSize());
                //             httpClientRead(this, result, true);
                //         }
                //         while (!httpClientEof(this));
                //     }
                // }
                // // Else create an io object, even if there is no content.  This makes the logic for readers easier -- they can just
                // // check eof rather than also checking if the io object exists.
                // else
                // {
                //     MEM_CONTEXT_BEGIN(this->memContext)
                //     {
                //         this->ioRead = ioReadNewP(this, .eof = httpClientEof, .read = httpClientRead);
                //         ioReadOpen(this->ioRead);
                //     }
                //     MEM_CONTEXT_END();
                // }
            }
            CATCH_ANY()
            {
                // Close the client
                httpClientDone(this, true, false);

                // Retry if wait time has not expired
                if (waitMore(wait))
                {
                    LOG_DEBUG_FMT("retry %s: %s", errorTypeName(errorType()), errorMessage());
                    retry = true;

                    httpClientStatLocal.retry++;
                }
                else
                    RETHROW();
            }
            TRY_END();
        }
        while (retry);

        httpClientStatLocal.request++;
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(HTTP_RESPONSE, result);
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
httpClientDone(HttpClient *this, bool close, bool closeRequired)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(HTTP_CLIENT, this);
        FUNCTION_LOG_PARAM(BOOL, close);
        FUNCTION_LOG_PARAM(BOOL, closeRequired);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    CHECK(this->busy);
    ASSERT(close || !closeRequired);

    // If it looks like we were in the middle of a response then close the TLS session so we can start clean next time
    if (close)
    {
        tlsSessionFree(this->tlsSession);
        this->tlsSession = NULL;

        // If a close was required by the server then increment stats
        if (closeRequired)
            httpClientStatLocal.close++;
    }

    this->busy = false;

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

    FUNCTION_TEST_RETURN(this->busy);
}
