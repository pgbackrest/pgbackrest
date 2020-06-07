/***********************************************************************************************************************************
Test Http
***********************************************************************************************************************************/
#include <unistd.h>

#include "common/io/handleRead.h"
#include "common/io/handleWrite.h"

#include "common/harnessFork.h"
#include "common/harnessTls.h"

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    // *****************************************************************************************************************************
    if (testBegin("httpUriEncode"))
    {
        TEST_RESULT_PTR(httpUriEncode(NULL, false), NULL, "null encodes to null");
        TEST_RESULT_STR_Z(httpUriEncode(strNew("0-9_~/A Z.az"), false), "0-9_~%2FA%20Z.az", "non-path encoding");
        TEST_RESULT_STR_Z(httpUriEncode(strNew("0-9_~/A Z.az"), true), "0-9_~/A%20Z.az", "path encoding");
    }

    // *****************************************************************************************************************************
    if (testBegin("httpLastModifiedToTime()"))
    {
        TEST_ERROR(httpLastModifiedToTime(STRDEF("Wed, 21 Bog 2015 07:28:00 GMT")), FormatError, "invalid month 'Bog'");
        TEST_ERROR(
            httpLastModifiedToTime(STRDEF("Wed,  1 Oct 2015 07:28:00 GMT")), FormatError,
            "unable to convert base 10 string ' 1' to int");
        TEST_RESULT_INT(httpLastModifiedToTime(STRDEF("Wed, 21 Oct 2015 07:28:00 GMT")), 1445412480, "convert gmt datetime");
    }

    // *****************************************************************************************************************************
    if (testBegin("HttpHeader"))
    {
        HttpHeader *header = NULL;

        MEM_CONTEXT_TEMP_BEGIN()
        {
            TEST_ASSIGN(header, httpHeaderNew(NULL), "new header");

            TEST_RESULT_PTR(httpHeaderMove(header, memContextPrior()), header, "move to new context");
            TEST_RESULT_PTR(httpHeaderMove(NULL, memContextPrior()), NULL, "move null to new context");
        }
        MEM_CONTEXT_TEMP_END();

        TEST_RESULT_PTR(httpHeaderAdd(header, strNew("key2"), strNew("value2")), header, "add header");
        TEST_RESULT_PTR(httpHeaderPut(header, strNew("key2"), strNew("value2a")), header, "put header");
        TEST_RESULT_PTR(httpHeaderAdd(header, strNew("key2"), strNew("value2b")), header, "add header");

        TEST_RESULT_PTR(httpHeaderAdd(header, strNew("key1"), strNew("value1")), header, "add header");
        TEST_RESULT_STR_Z(strLstJoin(httpHeaderList(header), ", "), "key1, key2", "header list");

        TEST_RESULT_STR_Z(httpHeaderGet(header, strNew("key1")), "value1", "get value");
        TEST_RESULT_STR_Z(httpHeaderGet(header, strNew("key2")), "value2a, value2b", "get value");
        TEST_RESULT_PTR(httpHeaderGet(header, strNew(BOGUS_STR)), NULL, "get missing value");

        TEST_RESULT_STR_Z(httpHeaderToLog(header), "{key1: 'value1', key2: 'value2a, value2b'}", "log output");

        TEST_RESULT_VOID(httpHeaderFree(header), "free header");

        // Redacted headers
        // -------------------------------------------------------------------------------------------------------------------------
        StringList *redact = strLstNew();
        strLstAddZ(redact, "secret");

        TEST_ASSIGN(header, httpHeaderNew(redact), "new header with redaction");
        httpHeaderAdd(header, strNew("secret"), strNew("secret-value"));
        httpHeaderAdd(header, strNew("public"), strNew("public-value"));

        TEST_RESULT_STR_Z(httpHeaderToLog(header), "{public: 'public-value', secret: <redacted>}", "log output");

        // Duplicate
        // -------------------------------------------------------------------------------------------------------------------------
        redact = strLstNew();
        strLstAddZ(redact, "public");

        TEST_RESULT_STR_Z(
            httpHeaderToLog(httpHeaderDup(header, NULL)), "{public: 'public-value', secret: <redacted>}",
            "dup and keep redactions");
        TEST_RESULT_STR_Z(
            httpHeaderToLog(httpHeaderDup(header, redact)), "{public: <redacted>, secret: 'secret-value'}",
            "dup and change redactions");
        TEST_RESULT_PTR(httpHeaderDup(NULL, NULL), NULL, "dup null http header");
    }

    // *****************************************************************************************************************************
    if (testBegin("HttpQuery"))
    {
        HttpQuery *query = NULL;

        MEM_CONTEXT_TEMP_BEGIN()
        {
            TEST_ASSIGN(query, httpQueryNew(), "new query");

            TEST_RESULT_PTR(httpQueryMove(query, memContextPrior()), query, "move to new context");
            TEST_RESULT_PTR(httpQueryMove(NULL, memContextPrior()), NULL, "move null to new context");
        }
        MEM_CONTEXT_TEMP_END();

        TEST_RESULT_PTR(httpQueryRender(NULL), NULL, "null query renders null");
        TEST_RESULT_PTR(httpQueryRender(query), NULL, "empty query renders null");

        TEST_RESULT_PTR(httpQueryAdd(query, strNew("key2"), strNew("value2")), query, "add query");
        TEST_ERROR(httpQueryAdd(query, strNew("key2"), strNew("value2")), AssertError, "key 'key2' already exists");
        TEST_RESULT_PTR(httpQueryPut(query, strNew("key2"), strNew("value2a")), query, "put query");
        TEST_RESULT_STR_Z(httpQueryRender(query), "key2=value2a", "render one query item");

        TEST_RESULT_PTR(httpQueryAdd(query, strNew("key1"), strNew("value 1?")), query, "add query");
        TEST_RESULT_STR_Z(strLstJoin(httpQueryList(query), ", "), "key1, key2", "query list");
        TEST_RESULT_STR_Z(httpQueryRender(query), "key1=value%201%3F&key2=value2a", "render two query items");

        TEST_RESULT_STR_Z(httpQueryGet(query, strNew("key1")), "value 1?", "get value");
        TEST_RESULT_STR_Z(httpQueryGet(query, strNew("key2")), "value2a", "get value");
        TEST_RESULT_PTR(httpQueryGet(query, strNew(BOGUS_STR)), NULL, "get missing value");

        TEST_RESULT_STR_Z(httpQueryToLog(query), "{key1: 'value 1?', key2: 'value2a'}", "log output");

        TEST_RESULT_VOID(httpQueryFree(query), "free query");
    }

    // *****************************************************************************************************************************
    if (testBegin("HttpClient"))
    {
        HttpClient *client = NULL;

        // Reset statistics
        httpClientStatLocal = (HttpClientStat){0};

        TEST_RESULT_PTR(httpClientStatStr(), NULL, "no stats yet");

        TEST_ASSIGN(
            client, httpClientNew(strNew("localhost"), hrnTlsServerPort(), 500, testContainer(), NULL, NULL),
            "new client");

        TEST_ERROR_FMT(
            httpClientRequest(client, strNew("GET"), strNew("/"), NULL, NULL, NULL, false), HostConnectError,
            "unable to connect to 'localhost:%u': [111] Connection refused", hrnTlsServerPort());

        HARNESS_FORK_BEGIN()
        {
            HARNESS_FORK_CHILD_BEGIN(0, true)
            {
                // Start http test server
                TEST_RESULT_VOID(
                    hrnTlsServerRun(ioHandleReadNew(strNew("test server read"), HARNESS_FORK_CHILD_READ(), 5000)),
                    "http server begin");
            }
            HARNESS_FORK_CHILD_END();

            HARNESS_FORK_PARENT_BEGIN()
            {
                hrnTlsClientBegin(ioHandleWriteNew(strNew("test client write"), HARNESS_FORK_PARENT_WRITE_PROCESS(0)));

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("create client");

                ioBufferSizeSet(35);

                TEST_ASSIGN(
                    client, httpClientNew(hrnTlsServerHost(), hrnTlsServerPort(), 5000, testContainer(), NULL, NULL),
                    "new client");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("no output from server");

                client->timeout = 0;

                hrnTlsServerAccept();

                hrnTlsServerExpectZ("GET / HTTP/1.1\r\n\r\n");
                hrnTlsServerSleep(600);

                hrnTlsServerClose();

                TEST_ERROR(
                    httpClientRequest(client, strNew("GET"), strNew("/"), NULL, NULL, NULL, false), FileReadError,
                    "unexpected eof while reading line");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("no CR at end of status");

                hrnTlsServerAccept();

                hrnTlsServerExpectZ("GET / HTTP/1.1\r\n\r\n");
                hrnTlsServerReplyZ("HTTP/1.0 200 OK\n");

                hrnTlsServerClose();

                TEST_ERROR(
                    httpClientRequest(client, strNew("GET"), strNew("/"), NULL, NULL, NULL, false), FormatError,
                    "http response status 'HTTP/1.0 200 OK' should be CR-terminated");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("status too short");

                hrnTlsServerAccept();

                hrnTlsServerExpectZ("GET / HTTP/1.1\r\n\r\n");
                hrnTlsServerReplyZ("HTTP/1.0 200\r\n");

                hrnTlsServerClose();

                TEST_ERROR(
                    httpClientRequest(client, strNew("GET"), strNew("/"), NULL, NULL, NULL, false), FormatError,
                    "http response 'HTTP/1.0 200' has invalid length");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("invalid http version");

                hrnTlsServerAccept();

                hrnTlsServerExpectZ("GET / HTTP/1.1\r\n\r\n");
                hrnTlsServerReplyZ("HTTP/1.0 200 OK\r\n");

                hrnTlsServerClose();

                TEST_ERROR(
                    httpClientRequest(client, strNew("GET"), strNew("/"), NULL, NULL, NULL, false), FormatError,
                    "http version of response 'HTTP/1.0 200 OK' must be HTTP/1.1");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("no space in status");

                hrnTlsServerAccept();

                hrnTlsServerExpectZ("GET / HTTP/1.1\r\n\r\n");
                hrnTlsServerReplyZ("HTTP/1.1 200OK\r\n");

                hrnTlsServerClose();

                TEST_ERROR(
                    httpClientRequest(client, strNew("GET"), strNew("/"), NULL, NULL, NULL, false), FormatError,
                    "response status '200OK' must have a space after the status code");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("unexpected end of headers");

                hrnTlsServerAccept();

                hrnTlsServerExpectZ("GET / HTTP/1.1\r\n\r\n");
                hrnTlsServerReplyZ("HTTP/1.1 200 OK\r\n");

                hrnTlsServerClose();

                TEST_ERROR(
                    httpClientRequest(client, strNew("GET"), strNew("/"), NULL, NULL, NULL, false), FileReadError,
                    "unexpected eof while reading line");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("missing colon in header");

                hrnTlsServerAccept();

                hrnTlsServerExpectZ("GET / HTTP/1.1\r\n\r\n");
                hrnTlsServerReplyZ("HTTP/1.1 200 OK\r\nheader-value\r\n");

                hrnTlsServerClose();

                TEST_ERROR(
                    httpClientRequest(client, strNew("GET"), strNew("/"), NULL, NULL, NULL, false), FormatError,
                    "header 'header-value' missing colon");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("invalid transfer encoding");

                hrnTlsServerAccept();

                hrnTlsServerExpectZ("GET / HTTP/1.1\r\n\r\n");
                hrnTlsServerReplyZ("HTTP/1.1 200 OK\r\ntransfer-encoding:bogus\r\n");

                hrnTlsServerClose();

                TEST_ERROR(
                    httpClientRequest(client, strNew("GET"), strNew("/"), NULL, NULL, NULL, false), FormatError,
                    "only 'chunked' is supported for 'transfer-encoding' header");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("content length and transfer encoding both set");

                hrnTlsServerAccept();

                hrnTlsServerExpectZ("GET / HTTP/1.1\r\n\r\n");
                hrnTlsServerReplyZ("HTTP/1.1 200 OK\r\ntransfer-encoding:chunked\r\ncontent-length:777\r\n\r\n");

                hrnTlsServerClose();

                TEST_ERROR(
                    httpClientRequest(client, strNew("GET"), strNew("/"), NULL, NULL, NULL, false), FormatError,
                    "'transfer-encoding' and 'content-length' headers are both set");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("5xx error with no retry");

                hrnTlsServerAccept();

                hrnTlsServerExpectZ("GET / HTTP/1.1\r\n\r\n");
                hrnTlsServerReplyZ("HTTP/1.1 503 Slow Down\r\n\r\n");

                hrnTlsServerClose();

                TEST_ERROR(
                    httpClientRequest(client, strNew("GET"), strNew("/"), NULL, NULL, NULL, false), ServiceError,
                    "[503] Slow Down");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("request with no content (with an internal error)");

                hrnTlsServerAccept();

                hrnTlsServerExpectZ("GET /?name=%2Fpath%2FA%20Z.txt&type=test HTTP/1.1\r\nhost:myhost.com\r\n\r\n");
                hrnTlsServerReplyZ("HTTP/1.1 500 Internal Error\r\nConnection:close\r\n\r\n");

                hrnTlsServerClose();
                hrnTlsServerAccept();

                hrnTlsServerExpectZ("GET /?name=%2Fpath%2FA%20Z.txt&type=test HTTP/1.1\r\nhost:myhost.com\r\n\r\n");
                hrnTlsServerReplyZ("HTTP/1.1 200 OK\r\nkey1:0\r\n key2 : value2\r\nConnection:ack\r\n\r\n");

                HttpHeader *headerRequest = httpHeaderNew(NULL);
                httpHeaderAdd(headerRequest, strNew("host"), strNew("myhost.com"));

                HttpQuery *query = httpQueryNew();
                httpQueryAdd(query, strNew("name"), strNew("/path/A Z.txt"));
                httpQueryAdd(query, strNew("type"), strNew("test"));

                client->timeout = 5000;

                HttpResponse *response = NULL;

                MEM_CONTEXT_TEMP_BEGIN()
                {
                    TEST_ASSIGN(
                        response, httpClientRequest(client, strNew("GET"), strNew("/"), query, headerRequest, NULL, false),
                        "request");
                    TEST_RESULT_VOID(httpResponseMove(response, memContextPrior()), "move response");
                }
                MEM_CONTEXT_TEMP_END();

                TEST_RESULT_UINT(httpResponseCode(response), 200, "check response code");
                TEST_RESULT_BOOL(httpResponseCodeOk(response), true, "check response code ok");
                TEST_RESULT_STR_Z(httpResponseReason(response), "OK", "check response message");
                TEST_RESULT_UINT(httpResponseEof(response), true, "io is eof");
                TEST_RESULT_STR_Z(
                    httpHeaderToLog(httpResponseHeader(response)),  "{connection: 'ack', key1: '0', key2: 'value2'}",
                    "check response headers");
                TEST_RESULT_UINT(bufSize(httpResponseContent(response)), 0, "content is empty");
                TEST_RESULT_VOID(httpResponseDone(response), "check done again");
                TEST_RESULT_VOID(httpResponseFree(response), "free");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("head request with content-length but no content");

                hrnTlsServerExpectZ("HEAD / HTTP/1.1\r\n\r\n");
                hrnTlsServerReplyZ("HTTP/1.1 200 OK\r\ncontent-length:380\r\n\r\n");

                TEST_ASSIGN(
                    response, httpClientRequest(client, strNew("HEAD"), strNew("/"), NULL, httpHeaderNew(NULL), NULL, true),
                    "request");
                TEST_RESULT_UINT(httpResponseCode(response), 200, "check response code");
                TEST_RESULT_STR_Z(httpResponseReason(response), "OK", "check response message");
                TEST_RESULT_BOOL(httpResponseEof(response), true, "io is eof");
                TEST_RESULT_BOOL(httpClientBusy(client), false, "client is not busy");
                TEST_RESULT_STR_Z(
                    httpHeaderToLog(httpResponseHeader(response)),  "{content-length: '380'}", "check response headers");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("head request with transfer encoding but no content");

                hrnTlsServerExpectZ("HEAD / HTTP/1.1\r\n\r\n");
                hrnTlsServerReplyZ("HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n");

                TEST_ASSIGN(
                    response, httpClientRequest(client, strNew("HEAD"), strNew("/"), NULL, httpHeaderNew(NULL), NULL, true),
                    "request");
                TEST_RESULT_UINT(httpResponseCode(response), 200, "check response code");
                TEST_RESULT_STR_Z(httpResponseReason(response), "OK", "check response message");
                TEST_RESULT_BOOL(httpResponseEof(response), true, "io is eof");
                TEST_RESULT_BOOL(httpClientBusy(client), false, "client is not busy");
                TEST_RESULT_STR_Z(
                    httpHeaderToLog(httpResponseHeader(response)),  "{transfer-encoding: 'chunked'}", "check response headers");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("head request with connection close but no content");

                hrnTlsServerExpectZ("HEAD / HTTP/1.1\r\n\r\n");
                hrnTlsServerReplyZ("HTTP/1.1 200 OK\r\nConnection:close\r\n\r\n");

                hrnTlsServerClose();

                TEST_ASSIGN(
                    response, httpClientRequest(client, strNew("HEAD"), strNew("/"), NULL, httpHeaderNew(NULL), NULL, true),
                    "request");
                TEST_RESULT_UINT(httpResponseCode(response), 200, "check response code");
                TEST_RESULT_STR_Z(httpResponseReason(response), "OK", "check response message");
                TEST_RESULT_BOOL(httpResponseEof(response), true, "io is eof");
                TEST_RESULT_BOOL(httpClientBusy(client), false, "client is not busy");
                TEST_RESULT_STR_Z(
                    httpHeaderToLog(httpResponseHeader(response)),  "{connection: 'close'}", "check response headers");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("error with content (with a few slow down errors)");

                hrnTlsServerAccept();

                hrnTlsServerExpectZ("GET / HTTP/1.1\r\n\r\n");
                hrnTlsServerReplyZ("HTTP/1.1 503 Slow Down\r\ncontent-length:3\r\nConnection:close\r\n\r\n123");

                hrnTlsServerClose();
                hrnTlsServerAccept();

                hrnTlsServerExpectZ("GET / HTTP/1.1\r\n\r\n");
                hrnTlsServerReplyZ("HTTP/1.1 503 Slow Down\r\nTransfer-Encoding:chunked\r\nConnection:close\r\n\r\n0\r\n\r\n");

                hrnTlsServerClose();
                hrnTlsServerAccept();

                hrnTlsServerExpectZ("GET / HTTP/1.1\r\n\r\n");
                hrnTlsServerReplyZ("HTTP/1.1 404 Not Found\r\ncontent-length:0\r\n\r\n");

                TEST_ASSIGN(response, httpClientRequest(client, strNew("GET"), strNew("/"), NULL, NULL, NULL, false), "request");
                TEST_RESULT_UINT(httpResponseCode(response), 404, "check response code");
                TEST_RESULT_BOOL(httpResponseCodeOk(response), false, "check response code error");
                TEST_RESULT_STR_Z(httpResponseReason(response), "Not Found", "check response message");
                TEST_RESULT_STR_Z(
                    httpHeaderToLog(httpResponseHeader(response)),  "{content-length: '0'}", "check response headers");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("error with content");

                hrnTlsServerExpectZ("GET / HTTP/1.1\r\n\r\n");
                hrnTlsServerReplyZ("HTTP/1.1 403 \r\ncontent-length:7\r\n\r\nCONTENT");

                TEST_ASSIGN(response, httpClientRequest(client, strNew("GET"), strNew("/"), NULL, NULL, NULL, false), "request");
                TEST_RESULT_UINT(httpResponseCode(response), 403, "check response code");
                TEST_RESULT_STR_Z(httpResponseReason(response), "", "check empty response message");
                TEST_RESULT_STR_Z(
                    httpHeaderToLog(httpResponseHeader(response)),  "{content-length: '7'}", "check response headers");
                TEST_RESULT_STR_Z(strNewBuf(httpResponseContent(response)),  "CONTENT", "check response content");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("request with content using content-length");

                hrnTlsServerExpectZ("GET /path/file%201.txt HTTP/1.1\r\ncontent-length:30\r\n\r\n012345678901234567890123456789");
                hrnTlsServerReplyZ("HTTP/1.1 200 OK\r\nConnection:close\r\n\r\n01234567890123456789012345678901");

                hrnTlsServerClose();

                ioBufferSizeSet(30);

                TEST_ASSIGN(
                    response,
                    httpClientRequest(
                        client, strNew("GET"), strNew("/path/file 1.txt"), NULL,
                        httpHeaderAdd(httpHeaderNew(NULL), strNew("content-length"), strNew("30")),
                        BUFSTRDEF("012345678901234567890123456789"), true),
                    "request");
                TEST_RESULT_STR_Z(
                    httpHeaderToLog(httpResponseHeader(response)),  "{connection: 'close'}", "check response headers");
                TEST_RESULT_STR_Z(strNewBuf(httpResponseContent(response)),  "01234567890123456789012345678901", "check response");
                TEST_RESULT_UINT(httpResponseRead(response, bufNew(1), true), 0, "call internal read to check eof");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("request with eof before content complete with retry");

                hrnTlsServerAccept();

                hrnTlsServerExpectZ("GET /path/file%201.txt HTTP/1.1\r\n\r\n");
                hrnTlsServerReplyZ("HTTP/1.1 200 OK\r\ncontent-length:32\r\n\r\n0123456789012345678901234567890");

                hrnTlsServerClose();
                hrnTlsServerAccept();

                hrnTlsServerExpectZ("GET /path/file%201.txt HTTP/1.1\r\n\r\n");
                hrnTlsServerReplyZ("HTTP/1.1 200 OK\r\ncontent-length:32\r\n\r\n01234567890123456789012345678901");

                TEST_ASSIGN(
                    response, httpClientRequest(client, strNew("GET"), strNew("/path/file 1.txt"), NULL, NULL, NULL, true),
                    "request");
                TEST_RESULT_STR_Z(strNewBuf(httpResponseContent(response)),  "01234567890123456789012345678901", "check response");
                TEST_RESULT_UINT(httpResponseRead(response, bufNew(1), true), 0, "call internal read to check eof");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("request with eof before content complete");

                hrnTlsServerExpectZ("GET /path/file%201.txt HTTP/1.1\r\n\r\n");
                hrnTlsServerReplyZ("HTTP/1.1 200 OK\r\ncontent-length:32\r\n\r\n0123456789012345678901234567890");

                hrnTlsServerClose();

                TEST_ASSIGN(
                    response, httpClientRequest(client, strNew("GET"), strNew("/path/file 1.txt"), NULL, NULL, NULL, false),
                    "request");
                TEST_RESULT_BOOL(httpClientBusy(client), true, "client is busy");
                TEST_ERROR(ioRead(httpResponseIoRead(response), bufNew(32)), FileReadError, "unexpected EOF reading HTTP content");
                TEST_RESULT_BOOL(httpClientBusy(client), false, "client is not busy");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("request with chunked content");

                hrnTlsServerAccept();

                hrnTlsServerExpectZ("GET / HTTP/1.1\r\n\r\n");
                hrnTlsServerReplyZ(
                    "HTTP/1.1 200 OK\r\nTransfer-Encoding:chunked\r\n\r\n"
                    "20\r\n01234567890123456789012345678901\r\n"
                    "10\r\n0123456789012345\r\n"
                    "0\r\n\r\n");

                TEST_ASSIGN(response, httpClientRequest(client, strNew("GET"), strNew("/"), NULL, NULL, NULL, false), "request");
                TEST_RESULT_STR_Z(
                    httpHeaderToLog(httpResponseHeader(response)),  "{transfer-encoding: 'chunked'}", "check response headers");

                Buffer *buffer = bufNew(35);

                TEST_RESULT_VOID(ioRead(httpResponseIoRead(response), buffer),  "read response");
                TEST_RESULT_STR_Z(strNewBuf(buffer),  "01234567890123456789012345678901012", "check response");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("close connection");

                hrnTlsServerClose();

                TEST_RESULT_VOID(httpClientFree(client), "free client");

                // -----------------------------------------------------------------------------------------------------------------
                hrnTlsClientEnd();
            }
            HARNESS_FORK_PARENT_END();
        }
        HARNESS_FORK_END();

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("statistics exist");

        TEST_RESULT_BOOL(httpClientStatStr() != NULL, true, "check");
    }

    // *****************************************************************************************************************************
    if (testBegin("HttpClientCache"))
    {
        HttpClientCache *cache = NULL;
        HttpClient *client1 = NULL;
        HttpClient *client2 = NULL;

        TEST_ASSIGN(
            cache, httpClientCacheNew(strNew("localhost"), hrnTlsServerPort(), 5000, true, NULL, NULL), "new http client cache");
        TEST_ASSIGN(client1, httpClientCacheGet(cache), "get http client");
        TEST_RESULT_PTR(client1, *(HttpClient **)lstGet(cache->clientList, 0), "    check http client");
        TEST_RESULT_PTR(httpClientCacheGet(cache), *(HttpClient **)lstGet(cache->clientList, 0), "    get same http client");

        // Make client 1 look like it is busy
        client1->response = (HttpResponse *)1;

        TEST_ASSIGN(client2, httpClientCacheGet(cache), "get http client");
        TEST_RESULT_PTR(client2, *(HttpClient **)lstGet(cache->clientList, 1), "    check http client");
        TEST_RESULT_BOOL(client1 != client2, true, "clients are not the same");

        // Set back to NULL so bad things don't happen during free
        client1->response = NULL;

        TEST_RESULT_VOID(httpClientCacheFree(cache), "free http client cache");
    }

    FUNCTION_HARNESS_RESULT_VOID();
}
