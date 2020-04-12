/***********************************************************************************************************************************
Test Http
***********************************************************************************************************************************/
#include <unistd.h>

#include "common/time.h"

#include "common/harnessFork.h"
#include "common/harnessTls.h"

/***********************************************************************************************************************************
Test server
***********************************************************************************************************************************/
static void
testHttpServer(void)
{
    FUNCTION_HARNESS_VOID();

    harnessTlsServerInitDefault();

    // Test no output from server
    harnessTlsServerAccept();

    harnessTlsServerExpect(
        "GET / HTTP/1.1\r\n"
        "\r\n");

    sleepMSec(600);
    harnessTlsServerClose();

    // Test invalid http version
    harnessTlsServerAccept();

    harnessTlsServerExpect(
        "GET / HTTP/1.1\r\n"
        "\r\n");

    harnessTlsServerReply(
        "HTTP/1.0 200 OK\r\n");

    harnessTlsServerClose();

    // Test no space in status
    harnessTlsServerAccept();

    harnessTlsServerExpect(
        "GET / HTTP/1.1\r\n"
        "\r\n");

    harnessTlsServerReply(
        "HTTP/1.1 200OK\r\n");

    harnessTlsServerClose();

    // // Test unexpected end of headers
    // harnessTlsServerAccept();
    //
    // harnessTlsServerExpect(
    //     "GET / HTTP/1.1\r\n"
    //     "\r\n");
    //
    // harnessTlsServerReply(
    //     "HTTP/1.1 200 OK\r\n");
    //
    // harnessTlsServerClose();

    // Test missing colon in header
    harnessTlsServerAccept();

    harnessTlsServerExpect(
        "GET / HTTP/1.1\r\n"
        "\r\n");

    harnessTlsServerReply(
        "HTTP/1.1 200 OK\r\n"
        "header-value\r\n");

    harnessTlsServerClose();

    // Test invalid transfer encoding
    harnessTlsServerAccept();

    harnessTlsServerExpect(
        "GET / HTTP/1.1\r\n"
        "\r\n");

    harnessTlsServerReply(
        "HTTP/1.1 200 OK\r\n"
        "transfer-encoding:bogus\r\n");

    harnessTlsServerClose();

    // Test content length and transfer encoding both set
    harnessTlsServerAccept();

    harnessTlsServerExpect(
        "GET / HTTP/1.1\r\n"
        "\r\n");

    harnessTlsServerReply(
        "HTTP/1.1 200 OK\r\n"
        "transfer-encoding:chunked\r\n"
        "content-length:777\r\n"
        "\r\n");

    harnessTlsServerClose();

    // Test 5xx error with no retry
    harnessTlsServerAccept();

    harnessTlsServerExpect(
        "GET / HTTP/1.1\r\n"
        "\r\n");

    harnessTlsServerReply(
        "HTTP/1.1 503 Slow Down\r\n"
        "\r\n");

    harnessTlsServerClose();

    // Request with no content (with an internal error)
    harnessTlsServerAccept();

    harnessTlsServerExpect(
        "GET /?name=%2Fpath%2FA%20Z.txt&type=test HTTP/1.1\r\n"
        "host:myhost.com\r\n"
        "\r\n");

    harnessTlsServerReply(
        "HTTP/1.1 500 Internal Error\r\n"
        "Connection:close\r\n"
        "\r\n");

    harnessTlsServerClose();

    harnessTlsServerAccept();

    harnessTlsServerExpect(
        "GET /?name=%2Fpath%2FA%20Z.txt&type=test HTTP/1.1\r\n"
        "host:myhost.com\r\n"
        "\r\n");

    harnessTlsServerReply(
        "HTTP/1.1 200 OK\r\n"
        "key1:0\r\n"
        " key2 : value2\r\n"
        "Connection:ack\r\n"
        "\r\n");

    // Head request with content-length but no content
    harnessTlsServerExpect(
        "HEAD / HTTP/1.1\r\n"
        "\r\n");

    harnessTlsServerReply(
        "HTTP/1.1 200 OK\r\n"
        "content-length:380\r\n"
        "\r\n");

    // Head request with transfer encoding but no content
    harnessTlsServerExpect(
        "HEAD / HTTP/1.1\r\n"
        "\r\n");

    harnessTlsServerReply(
        "HTTP/1.1 200 OK\r\n"
        "Transfer-Encoding: chunked\r\n"
        "\r\n");

    // Head request with connection close but no content
    harnessTlsServerExpect(
        "HEAD / HTTP/1.1\r\n"
        "\r\n");

    harnessTlsServerReply(
        "HTTP/1.1 200 OK\r\n"
        "Connection:close\r\n"
        "\r\n");

    harnessTlsServerClose();

    harnessTlsServerAccept();

    // Error with content (with a few slow down errors)
    harnessTlsServerExpect(
        "GET / HTTP/1.1\r\n"
        "\r\n");

    harnessTlsServerReply(
        "HTTP/1.1 503 Slow Down\r\n"
        "content-length:3\r\n"
        "Connection:close\r\n"
        "\r\n"
        "123");

    harnessTlsServerClose();

    harnessTlsServerAccept();

    harnessTlsServerExpect(
        "GET / HTTP/1.1\r\n"
        "\r\n");

    harnessTlsServerReply(
        "HTTP/1.1 503 Slow Down\r\n"
        "Transfer-Encoding:chunked\r\n"
        "Connection:close\r\n"
        "\r\n"
        "0\r\n"
        "\r\n");

    harnessTlsServerClose();

    harnessTlsServerAccept();

    harnessTlsServerExpect(
        "GET / HTTP/1.1\r\n"
        "\r\n");

    harnessTlsServerReply(
        "HTTP/1.1 404 Not Found\r\n"
        "content-length:0\r\n"
        "\r\n");

    // Error with content
    harnessTlsServerAccept();

    harnessTlsServerExpect(
        "GET / HTTP/1.1\r\n"
        "\r\n");

    harnessTlsServerReply(
        "HTTP/1.1 403 Auth Error\r\n"
        "content-length:7\r\n"
        "\r\n"
        "CONTENT");

    // Request with content
    harnessTlsServerAccept();

    harnessTlsServerExpect(
        "GET /path/file%201.txt HTTP/1.1\r\n"
        "content-length:30\r\n"
        "\r\n"
        "012345678901234567890123456789");

    harnessTlsServerReply(
        "HTTP/1.1 200 OK\r\n"
        "Connection:close\r\n"
        "\r\n"
        "01234567890123456789012345678901");

    harnessTlsServerClose();

    // Request with eof before content complete with retry
    harnessTlsServerAccept();

    harnessTlsServerExpect(
        "GET /path/file%201.txt HTTP/1.1\r\n"
        "\r\n");

    harnessTlsServerReply(
        "HTTP/1.1 200 OK\r\n"
        "content-length:32\r\n"
        "\r\n"
        "0123456789012345678901234567890");

    harnessTlsServerClose();

    harnessTlsServerAccept();

    harnessTlsServerExpect(
        "GET /path/file%201.txt HTTP/1.1\r\n"
        "\r\n");

    harnessTlsServerReply(
        "HTTP/1.1 200 OK\r\n"
        "content-length:32\r\n"
        "\r\n"
        "01234567890123456789012345678901");

    // Request with eof before content complete
    harnessTlsServerAccept();

    harnessTlsServerExpect(
        "GET /path/file%201.txt HTTP/1.1\r\n"
        "\r\n");

    harnessTlsServerReply(
        "HTTP/1.1 200 OK\r\n"
        "content-length:32\r\n"
        "\r\n"
        "0123456789012345678901234567890");

    harnessTlsServerClose();

    // Request with chunked content
    harnessTlsServerAccept();

    harnessTlsServerExpect(
        "GET / HTTP/1.1\r\n"
        "\r\n");

    harnessTlsServerReply(
        "HTTP/1.1 200 OK\r\n"
        "Transfer-Encoding:chunked\r\n"
        "\r\n"
        "20\r\n"
        "01234567890123456789012345678901\r\n"
        "10\r\n"
        "0123456789012345\r\n"
        "0\r\n"
        "\r\n");

    harnessTlsServerClose();

    FUNCTION_HARNESS_RESULT_VOID();
}

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
        ioBufferSizeSet(35);

        // Reset statistics
        httpClientStatLocal = (HttpClientStat){0};

        TEST_RESULT_PTR(httpClientStatStr(), NULL, "no stats yet");

        TEST_ASSIGN(
            client, httpClientNew(strNew("localhost"), harnessTlsTestPort(), 500, testContainer(), NULL, NULL),
            "new client");

        TEST_ERROR_FMT(
            httpClientRequest(client, strNew("GET"), strNew("/"), NULL, NULL, NULL, false), HostConnectError,
            "unable to connect to 'localhost:%u': [111] Connection refused", harnessTlsTestPort());

        HARNESS_FORK_BEGIN()
        {
            HARNESS_FORK_CHILD_BEGIN(0, false)
            {
                // Start http test server
                TEST_RESULT_VOID(testHttpServer(), "http server begin");
            }
            HARNESS_FORK_CHILD_END();

            HARNESS_FORK_PARENT_BEGIN()
            {
                // Test no output from server
                TEST_ASSIGN(
                    client, httpClientNew(harnessTlsTestHost(), harnessTlsTestPort(), 2000, testContainer(), NULL, NULL),
                    "new client");
                client->timeout = 0;

                TEST_ERROR(
                    httpClientRequest(client, strNew("GET"), strNew("/"), NULL, NULL, NULL, false), FileReadError,
                    "unexpected eof while reading line");

                // Test invalid http version
                TEST_ERROR(
                    httpClientRequest(client, strNew("GET"), strNew("/"), NULL, NULL, NULL, false), FormatError,
                    "http version of response 'HTTP/1.0 200 OK' must be HTTP/1.1");

                // Test no space in status
                TEST_ERROR(
                    httpClientRequest(client, strNew("GET"), strNew("/"), NULL, NULL, NULL, false), FormatError,
                    "response status '200OK' must have a space");

                // // Test unexpected end of headers
                // TEST_ERROR(
                //     httpClientRequest(client, strNew("GET"), strNew("/"), NULL, NULL, NULL, false), FileReadError,
                //     "unexpected eof while reading line");

                // Test missing colon in header
                TEST_ERROR(
                    httpClientRequest(client, strNew("GET"), strNew("/"), NULL, NULL, NULL, false), FormatError,
                    "header 'header-value' missing colon");

                // Test invalid transfer encoding
                TEST_ERROR(
                    httpClientRequest(client, strNew("GET"), strNew("/"), NULL, NULL, NULL, false), FormatError,
                    "only 'chunked' is supported for 'transfer-encoding' header");

                // Test content length and transfer encoding both set
                TEST_ERROR(
                    httpClientRequest(client, strNew("GET"), strNew("/"), NULL, NULL, NULL, false), FormatError,
                    "'transfer-encoding' and 'content-length' headers are both set");

                // Test 5xx error with no retry
                TEST_ERROR(
                    httpClientRequest(client, strNew("GET"), strNew("/"), NULL, NULL, NULL, false), ServiceError,
                    "[503] Slow Down");

                // Request with no content
                client->timeout = 2000;

                HttpHeader *headerRequest = httpHeaderNew(NULL);
                httpHeaderAdd(headerRequest, strNew("host"), strNew("myhost.com"));

                HttpQuery *query = httpQueryNew();
                httpQueryAdd(query, strNew("name"), strNew("/path/A Z.txt"));
                httpQueryAdd(query, strNew("type"), strNew("test"));

                TEST_RESULT_VOID(
                    httpClientRequest(client, strNew("GET"), strNew("/"), query, headerRequest, NULL, false),
                    "request with no content");
                TEST_RESULT_UINT(httpClientResponseCode(client), 200, "    check response code");
                TEST_RESULT_STR_Z(httpClientResponseMessage(client), "OK", "    check response message");
                TEST_RESULT_UINT(httpClientEof(client), true, "    io is eof");
                TEST_RESULT_STR_Z(
                    httpHeaderToLog(httpClientResponseHeader(client)),  "{connection: 'ack', key1: '0', key2: 'value2'}",
                    "    check response headers");

                // Head request with content-length but no content
                TEST_RESULT_VOID(
                    httpClientRequest(client, strNew("HEAD"), strNew("/"), NULL, httpHeaderNew(NULL), NULL, true),
                    "head request with content-length");
                TEST_RESULT_UINT(httpClientResponseCode(client), 200, "    check response code");
                TEST_RESULT_STR_Z(httpClientResponseMessage(client), "OK", "    check response message");
                TEST_RESULT_BOOL(httpClientEof(client), true, "    io is eof");
                TEST_RESULT_BOOL(httpClientBusy(client), false, "    client is not busy");
                TEST_RESULT_STR_Z(
                    httpHeaderToLog(httpClientResponseHeader(client)),  "{content-length: '380'}", "    check response headers");

                // Head request with transfer encoding but no content
                TEST_RESULT_VOID(
                    httpClientRequest(client, strNew("HEAD"), strNew("/"), NULL, httpHeaderNew(NULL), NULL, true),
                    "head request with transfer encoding");
                TEST_RESULT_UINT(httpClientResponseCode(client), 200, "    check response code");
                TEST_RESULT_STR_Z(httpClientResponseMessage(client), "OK", "    check response message");
                TEST_RESULT_BOOL(httpClientEof(client), true, "    io is eof");
                TEST_RESULT_BOOL(httpClientBusy(client), false, "    client is not busy");
                TEST_RESULT_STR_Z(
                    httpHeaderToLog(httpClientResponseHeader(client)),  "{transfer-encoding: 'chunked'}",
                    "    check response headers");

                // Head request with connection close but no content
                TEST_RESULT_VOID(
                    httpClientRequest(client, strNew("HEAD"), strNew("/"), NULL, httpHeaderNew(NULL), NULL, true),
                    "head request with connection close");
                TEST_RESULT_UINT(httpClientResponseCode(client), 200, "    check response code");
                TEST_RESULT_STR_Z(httpClientResponseMessage(client), "OK", "    check response message");
                TEST_RESULT_BOOL(httpClientEof(client), true, "    io is eof");
                TEST_RESULT_BOOL(httpClientBusy(client), false, "    client is not busy");
                TEST_RESULT_STR_Z(
                    httpHeaderToLog(httpClientResponseHeader(client)),  "{connection: 'close'}", "    check response headers");

                // Error with content length 0
                TEST_RESULT_VOID(
                    httpClientRequest(client, strNew("GET"), strNew("/"), NULL, NULL, NULL, false), "error with content length 0");
                TEST_RESULT_UINT(httpClientResponseCode(client), 404, "    check response code");
                TEST_RESULT_STR_Z(httpClientResponseMessage(client), "Not Found", "    check response message");
                TEST_RESULT_STR_Z(
                    httpHeaderToLog(httpClientResponseHeader(client)),  "{content-length: '0'}", "    check response headers");

                // Error with content
                Buffer *buffer = NULL;

                TEST_ASSIGN(
                    buffer, httpClientRequest(client, strNew("GET"), strNew("/"), NULL, NULL, NULL, false),
                    "error with content length");
                TEST_RESULT_UINT(httpClientResponseCode(client), 403, "    check response code");
                TEST_RESULT_STR_Z(httpClientResponseMessage(client), "Auth Error", "    check response message");
                TEST_RESULT_STR_Z(
                    httpHeaderToLog(httpClientResponseHeader(client)),  "{content-length: '7'}", "    check response headers");
                TEST_RESULT_STR_Z(strNewBuf(buffer),  "CONTENT", "    check response");

                // Request with content using content-length
                ioBufferSizeSet(30);

                TEST_ASSIGN(
                    buffer,
                    httpClientRequest(
                        client, strNew("GET"), strNew("/path/file 1.txt"), NULL,
                        httpHeaderAdd(httpHeaderNew(NULL), strNew("content-length"), strNew("30")),
                        BUFSTRDEF("012345678901234567890123456789"), true),
                    "request with content length");
                TEST_RESULT_STR_Z(
                    httpHeaderToLog(httpClientResponseHeader(client)),  "{connection: 'close'}",
                    "    check response headers");
                TEST_RESULT_STR_Z(strNewBuf(buffer),  "01234567890123456789012345678901", "    check response");
                TEST_RESULT_UINT(httpClientRead(client, bufNew(1), true), 0, "    call internal read to check eof");

                // Request with eof before content complete with retry
                TEST_ASSIGN(
                    buffer, httpClientRequest(client, strNew("GET"), strNew("/path/file 1.txt"), NULL, NULL, NULL, true),
                    "request with content length retry");
                TEST_RESULT_STR_Z(strNewBuf(buffer),  "01234567890123456789012345678901", "    check response");
                TEST_RESULT_UINT(httpClientRead(client, bufNew(1), true), 0, "    call internal read to check eof");

                // Request with eof before content and error
                buffer = bufNew(32);
                TEST_RESULT_VOID(
                    httpClientRequest(client, strNew("GET"), strNew("/path/file 1.txt"), NULL, NULL, NULL, false),
                    "request with content length error");
                TEST_RESULT_BOOL(httpClientBusy(client), true, "    client is busy");
                TEST_ERROR(
                    ioRead(httpClientIoRead(client), buffer), FileReadError, "unexpected EOF reading HTTP content");

                // Request with content using chunked encoding
                TEST_RESULT_VOID(
                    httpClientRequest(client, strNew("GET"), strNew("/"), NULL, NULL, NULL, false),
                    "request with chunked encoding");
                TEST_RESULT_STR_Z(
                    httpHeaderToLog(httpClientResponseHeader(client)),  "{transfer-encoding: 'chunked'}",
                    "    check response headers");

                buffer = bufNew(35);
                TEST_RESULT_VOID(ioRead(httpClientIoRead(client), buffer),  "    read response");
                TEST_RESULT_STR_Z(strNewBuf(buffer),  "01234567890123456789012345678901012", "    check response");

                TEST_RESULT_BOOL(httpClientStatStr() != NULL, true, "check statistics exist");

                TEST_RESULT_VOID(httpClientFree(client), "free client");
            }
            HARNESS_FORK_PARENT_END();
        }
        HARNESS_FORK_END();
    }

    // *****************************************************************************************************************************
    if (testBegin("HttpClientCache"))
    {
        HttpClientCache *cache = NULL;
        HttpClient *client1 = NULL;
        HttpClient *client2 = NULL;

        TEST_ASSIGN(
            cache, httpClientCacheNew(strNew("localhost"), harnessTlsTestPort(), 2000, true, NULL, NULL), "new http client cache");
        TEST_ASSIGN(client1, httpClientCacheGet(cache), "get http client");
        TEST_RESULT_PTR(client1, *(HttpClient **)lstGet(cache->clientList, 0), "    check http client");
        TEST_RESULT_PTR(httpClientCacheGet(cache), *(HttpClient **)lstGet(cache->clientList, 0), "    get same http client");

        // Make client 1 look like it is busy
        client1->ioRead = (IoRead *)1;

        TEST_ASSIGN(client2, httpClientCacheGet(cache), "get http client");
        TEST_RESULT_PTR(client2, *(HttpClient **)lstGet(cache->clientList, 1), "    check http client");
        TEST_RESULT_BOOL(client1 != client2, true, "clients are not the same");

        // Set back to NULL so bad things don't happen during free
        client1->ioRead = NULL;

        TEST_RESULT_VOID(httpClientCacheFree(cache), "free http client cache");
    }

    FUNCTION_HARNESS_RESULT_VOID();
}
