/***********************************************************************************************************************************
Test HTTP
***********************************************************************************************************************************/
#include <unistd.h>

#include "common/io/fdRead.h"
#include "common/io/fdWrite.h"
#include "common/io/tls/client.h"
#include "common/io/socket/client.h"

#include "common/harnessFork.h"
#include "common/harnessServer.h"

/***********************************************************************************************************************************
HTTP user agent header
***********************************************************************************************************************************/
#define TEST_USER_AGENT                                                                                                            \
    HTTP_HEADER_USER_AGENT ":" PROJECT_NAME "/" PROJECT_VERSION "\r\n"

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    // *****************************************************************************************************************************
    if (testBegin("httpUriEncode() and httpUriDecode()"))
    {
        TEST_RESULT_STR(httpUriEncode(NULL, false), NULL, "null encodes to null");
        TEST_RESULT_STR_Z(httpUriEncode(STRDEF("0-9_~/A Z.az"), false), "0-9_~%2FA%20Z.az", "non-path encoding");
        TEST_RESULT_STR_Z(httpUriEncode(STRDEF("0-9_~/A Z.az"), true), "0-9_~/A%20Z.az", "path encoding");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("decode");

        TEST_RESULT_STR(httpUriDecode(NULL), NULL, "null decodes to null");
        TEST_RESULT_STR_Z(httpUriDecode(STRDEF("0-9_~%2FA%20Z.az")), "0-9_~/A Z.az", "valid decode");
        TEST_ERROR(httpUriDecode(STRDEF("%A")), FormatError, "invalid escape sequence length in '%A'");
        TEST_ERROR(httpUriDecode(STRDEF("%XX")), FormatError, "unable to convert base 16 string 'XX' to unsigned int");
    }

    // *****************************************************************************************************************************
    if (testBegin("httpDateToTime() and httpDateFromTime()"))
    {
        TEST_ERROR(httpDateToTime(STRDEF("Wed, 21 Bog 2015 07:28:00 GMT")), FormatError, "invalid month 'Bog'");
        TEST_ERROR(
            httpDateToTime(STRDEF("Wed,  1 Oct 2015 07:28:00 GMT")), FormatError, "unable to convert base 10 string ' 1' to int");
        TEST_RESULT_INT(httpDateToTime(STRDEF("Wed, 21 Oct 2015 07:28:00 GMT")), 1445412480, "convert HTTP date to time_t");

        TEST_RESULT_STR_Z(httpDateFromTime(1592743579), "Sun, 21 Jun 2020 12:46:19 GMT", "convert time_t to HTTP date");
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

        TEST_RESULT_PTR(httpHeaderAdd(header, STRDEF("key2"), STRDEF("value2")), header, "add header");
        TEST_RESULT_PTR(httpHeaderPut(header, STRDEF("key2"), STRDEF("value2a")), header, "put header");
        TEST_RESULT_PTR(httpHeaderAdd(header, STRDEF("key2"), STRDEF("value2b")), header, "add header");

        TEST_RESULT_PTR(httpHeaderAdd(header, STRDEF("key1"), STRDEF("value1")), header, "add header");
        TEST_RESULT_STRLST_Z(httpHeaderList(header), "key1\nkey2\n", "header list");

        TEST_RESULT_STR_Z(httpHeaderGet(header, STRDEF("key1")), "value1", "get value");
        TEST_RESULT_STR_Z(httpHeaderGet(header, STRDEF("key2")), "value2a, value2b", "get value");
        TEST_RESULT_STR(httpHeaderGet(header, STRDEF(BOGUS_STR)), NULL, "get missing value");

        TEST_RESULT_STR_Z(httpHeaderToLog(header), "{key1: 'value1', key2: 'value2a, value2b'}", "log output");

        TEST_RESULT_VOID(httpHeaderFree(header), "free header");

        // Redacted headers
        // -------------------------------------------------------------------------------------------------------------------------
        StringList *redact = strLstNew();
        strLstAddZ(redact, "secret");

        TEST_ASSIGN(header, httpHeaderNew(redact), "new header with redaction");
        httpHeaderAdd(header, STRDEF("secret"), STRDEF("secret-value"));
        httpHeaderAdd(header, STRDEF("public"), STRDEF("public-value"));

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
        TEST_RESULT_PTR(httpHeaderDup(NULL, NULL), NULL, "dup null header");
    }

    // *****************************************************************************************************************************
    if (testBegin("HttpQuery"))
    {
        HttpQuery *query = NULL;

        MEM_CONTEXT_TEMP_BEGIN()
        {
            StringList *redactList = strLstNew();
            strLstAdd(redactList, STRDEF("key2"));

            TEST_ASSIGN(query, httpQueryNewP(.redactList = redactList), "new query");

            TEST_RESULT_PTR(httpQueryMove(query, memContextPrior()), query, "move to new context");
            TEST_RESULT_PTR(httpQueryMove(NULL, memContextPrior()), NULL, "move null to new context");
        }
        MEM_CONTEXT_TEMP_END();

        TEST_RESULT_STR(httpQueryRenderP(NULL), NULL, "null query renders null");
        TEST_RESULT_STR(httpQueryRenderP(query), NULL, "empty query renders null");

        TEST_RESULT_PTR(httpQueryAdd(query, STRDEF("key2"), STRDEF("value2")), query, "add query");
        TEST_ERROR(httpQueryAdd(query, STRDEF("key2"), STRDEF("value2")), AssertError, "key 'key2' already exists");
        TEST_RESULT_PTR(httpQueryPut(query, STRDEF("key2"), STRDEF("value2a")), query, "put query");
        TEST_RESULT_STR_Z(httpQueryRenderP(query), "key2=value2a", "render one query item");

        TEST_RESULT_PTR(httpQueryAdd(query, STRDEF("key1"), STRDEF("value 1?")), query, "add query");
        TEST_RESULT_STRLST_Z(httpQueryList(query), "key1\nkey2\n", "query list");
        TEST_RESULT_STR_Z(httpQueryRenderP(query), "key1=value%201%3F&key2=value2a", "render two query items");
        TEST_RESULT_STR_Z(
            httpQueryRenderP(query, .redact = true), "key1=value%201%3F&key2=<redacted>", "render two query items with redaction");

        TEST_RESULT_STR_Z(httpQueryGet(query, STRDEF("key1")), "value 1?", "get value");
        TEST_RESULT_STR_Z(httpQueryGet(query, STRDEF("key2")), "value2a", "get value");
        TEST_RESULT_STR(httpQueryGet(query, STRDEF(BOGUS_STR)), NULL, "get missing value");

        TEST_RESULT_STR_Z(httpQueryToLog(query), "{key1: 'value 1?', key2: <redacted>}", "log output");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("dup query with redaction");

        StringList *redactList = strLstNew();
        strLstAdd(redactList, STRDEF("key1"));

        TEST_ASSIGN(query, httpQueryDupP(query, .redactList = redactList), "dup query");
        TEST_RESULT_STR_Z(httpQueryToLog(query), "{key1: <redacted>, key2: 'value2a'}", "log output");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("new query from string");

        TEST_ERROR(httpQueryNewStr(STRDEF("a=b&c")), FormatError, "invalid key/value 'c' in query 'a=b&c'");

        HttpQuery *query2 = NULL;
        TEST_ASSIGN(query2, httpQueryNewStr(STRDEF("?a=%2Bb&c=d%3D")), "query from string");
        TEST_RESULT_STR_Z(httpQueryRenderP(query2), "a=%2Bb&c=d%3D", "render query");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("merge queries");

        TEST_RESULT_STR_Z(
            httpQueryRenderP(httpQueryMerge(query, query2)), "a=%2Bb&c=d%3D&key1=value%201%3F&key2=value2a", "render merge");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("free query");

        TEST_RESULT_VOID(httpQueryFree(query), "free");
    }

    // *****************************************************************************************************************************
    if (testBegin("HttpUrl"))
    {
        HttpUrl *url = NULL;

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("invalid url");

        TEST_ERROR(httpUrlNewParseP(STRDEF("ftp://" BOGUS_STR)), FormatError, "invalid URL 'ftp://BOGUS'");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("HttpProtocolTypeStr");

        TEST_RESULT_STR_Z(httpProtocolTypeStr(httpProtocolTypeHttp), "http", "check http");
        TEST_RESULT_STR_Z(httpProtocolTypeStr(httpProtocolTypeAny), NULL, "check any");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("simple http");

        TEST_ASSIGN(url, httpUrlNewParseP(STRDEF("http://test"), .type = httpProtocolTypeHttp), "new");
        TEST_RESULT_STR_Z(httpUrl(url), "http://test", "check url");
        TEST_RESULT_STR_Z(httpUrlHost(url), "test", "check host");
        TEST_RESULT_STR_Z(httpUrlPath(url), "/", "check path");
        TEST_RESULT_UINT(httpUrlPort(url), 80, "check port");
        TEST_RESULT_UINT(httpUrlProtocolType(url), httpProtocolTypeHttp, "check protocol");
        TEST_RESULT_STR_Z(httpUrlToLog(url), "{http://test:80/}", "check log");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("host and port");

        TEST_ASSIGN(url, httpUrlNewParseP(STRDEF("gcs:4443"), .type = httpProtocolTypeHttps), "new");
        TEST_RESULT_STR_Z(httpUrl(url), "gcs:4443", "check url");
        TEST_RESULT_STR_Z(httpUrlHost(url), "gcs", "check host");
        TEST_RESULT_STR_Z(httpUrlPath(url), "/", "check path");
        TEST_RESULT_UINT(httpUrlPort(url), 4443, "check port");
        TEST_RESULT_UINT(httpUrlProtocolType(url), httpProtocolTypeHttps, "check protocol");
        TEST_RESULT_STR_Z(httpUrlToLog(url), "{https://gcs:4443/}", "check log");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("http but expected https");

        TEST_ERROR(
            httpUrlNewParseP(STRDEF("http://test"), .type = httpProtocolTypeHttps), FormatError,
            "expected protocol 'https' in URL 'http://test'");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("https with port and path");

        TEST_ASSIGN(url, httpUrlNewParseP(STRDEF("https://test.com:445/path")), "new");
        TEST_RESULT_STR_Z(httpUrl(url), "https://test.com:445/path", "check url");
        TEST_RESULT_STR_Z(httpUrlHost(url), "test.com", "check host");
        TEST_RESULT_STR_Z(httpUrlPath(url), "/path", "check path");
        TEST_RESULT_UINT(httpUrlPort(url), 445, "check port");
        TEST_RESULT_UINT(httpUrlProtocolType(url), httpProtocolTypeHttps, "check protocol");
        TEST_RESULT_STR_Z(httpUrlToLog(url), "{https://test.com:445/path}", "check log");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("host only");

        TEST_ASSIGN(url, httpUrlNewParseP(STRDEF("test.com"), .type = httpProtocolTypeHttps), "new");
        TEST_RESULT_STR_Z(httpUrl(url), "test.com", "check url");
        TEST_RESULT_STR_Z(httpUrlHost(url), "test.com", "check host");
        TEST_RESULT_STR_Z(httpUrlPath(url), "/", "check path");
        TEST_RESULT_UINT(httpUrlPort(url), 443, "check port");
        TEST_RESULT_UINT(httpUrlProtocolType(url), httpProtocolTypeHttps, "check protocol");
        TEST_RESULT_STR_Z(httpUrlToLog(url), "{https://test.com:443/}", "check log");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("IPv6");

        TEST_ASSIGN(url, httpUrlNewParseP(STRDEF("http://[2001:db8::ff00:42:8329]:81"), .type = httpProtocolTypeHttp), "new");
        TEST_RESULT_STR_Z(httpUrl(url), "http://[2001:db8::ff00:42:8329]:81", "check url");
        TEST_RESULT_STR_Z(httpUrlHost(url), "2001:db8::ff00:42:8329", "check host");
        TEST_RESULT_STR_Z(httpUrlPath(url), "/", "check path");
        TEST_RESULT_UINT(httpUrlPort(url), 81, "check port");
        TEST_RESULT_UINT(httpUrlProtocolType(url), httpProtocolTypeHttp, "check protocol");
        TEST_RESULT_STR_Z(httpUrlToLog(url), "{http://[2001:db8::ff00:42:8329]:81/}", "check log");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("IPv6 no port");

        TEST_ASSIGN(url, httpUrlNewParseP(STRDEF("http://[2001:db8::ff00:42:8329]/url"), .type = httpProtocolTypeHttp), "new");
        TEST_RESULT_STR_Z(httpUrl(url), "http://[2001:db8::ff00:42:8329]/url", "check url");
        TEST_RESULT_STR_Z(httpUrlHost(url), "2001:db8::ff00:42:8329", "check host");
        TEST_RESULT_STR_Z(httpUrlPath(url), "/url", "check path");
        TEST_RESULT_UINT(httpUrlPort(url), 80, "check port");
        TEST_RESULT_UINT(httpUrlProtocolType(url), httpProtocolTypeHttp, "check protocol");
        TEST_RESULT_STR_Z(httpUrlToLog(url), "{http://[2001:db8::ff00:42:8329]:80/url}", "check log");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("free");

        TEST_RESULT_VOID(httpUrlFree(url), "free");
    }

    // *****************************************************************************************************************************
    if (testBegin("HttpClient"))
    {
        HttpClient *client = NULL;

        TEST_ASSIGN(client, httpClientNew(sckClientNew(STRDEF("localhost"), hrnServerPort(0), 500), 500), "new client");

        TEST_ERROR_FMT(
            httpRequestResponse(httpRequestNewP(client, STRDEF("GET"), STRDEF("/")), false), HostConnectError,
            "unable to connect to 'localhost:%u': [111] Connection refused", hrnServerPort(0));

        HRN_FORK_BEGIN()
        {
            HRN_FORK_CHILD_BEGIN(.prefix = "test server", .timeout = 5000)
            {
                // Start HTTP test server
                TEST_RESULT_VOID(hrnServerRunP(HRN_FORK_CHILD_READ(), hrnServerProtocolSocket), "http server run");
            }
            HRN_FORK_CHILD_END();

            HRN_FORK_PARENT_BEGIN()
            {
                IoWrite *http = hrnServerScriptBegin(HRN_FORK_PARENT_WRITE(0));

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("create client");

                ioBufferSizeSet(35);

                TEST_ASSIGN(client, httpClientNew(sckClientNew(hrnServerHost(), hrnServerPort(0), 5000), 5000), "new client");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("no output from server");

                client->pub.timeout = 0;

                hrnServerScriptAccept(http);

                hrnServerScriptExpectZ(http, "GET / HTTP/1.1\r\n" TEST_USER_AGENT "\r\n");
                hrnServerScriptSleep(http, 600);

                hrnServerScriptClose(http);

                TEST_ERROR(
                    httpRequestResponse(httpRequestNewP(client, STRDEF("GET"), STRDEF("/")), false), FileReadError,
                    "unexpected eof while reading line");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("no CR at end of status");

                hrnServerScriptAccept(http);

                hrnServerScriptExpectZ(http, "GET / HTTP/1.1\r\n" TEST_USER_AGENT "\r\n");
                hrnServerScriptReplyZ(http, "HTTP/1.0 200 OK\n");

                hrnServerScriptClose(http);

                TEST_ERROR(
                    httpRequestResponse(httpRequestNewP(client, STRDEF("GET"), STRDEF("/")), false), FormatError,
                    "HTTP response status 'HTTP/1.0 200 OK' should be CR-terminated");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("status too short");

                hrnServerScriptAccept(http);

                hrnServerScriptExpectZ(http, "GET / HTTP/1.1\r\n" TEST_USER_AGENT "\r\n");
                hrnServerScriptReplyZ(http, "HTTP/1.0 200\r\n");

                hrnServerScriptClose(http);

                TEST_ERROR(
                    httpRequestResponse(httpRequestNewP(client, STRDEF("GET"), STRDEF("/")), false), FormatError,
                    "HTTP response 'HTTP/1.0 200' has invalid length");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("invalid HTTP version");

                hrnServerScriptAccept(http);

                hrnServerScriptExpectZ(http, "GET / HTTP/1.1\r\n" TEST_USER_AGENT "\r\n");
                hrnServerScriptReplyZ(http, "HTTP/1 200 OK\r\n");

                hrnServerScriptClose(http);

                TEST_ERROR(
                    httpRequestResponse(httpRequestNewP(client, STRDEF("GET"), STRDEF("/")), false), FormatError,
                    "HTTP version of response 'HTTP/1 200 OK' must be HTTP/1.1 or HTTP/1.0");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("no space in status");

                hrnServerScriptAccept(http);

                hrnServerScriptExpectZ(http, "GET / HTTP/1.1\r\n" TEST_USER_AGENT "\r\n");
                hrnServerScriptReplyZ(http, "HTTP/1.1 200OK\r\n");

                hrnServerScriptClose(http);

                TEST_ERROR(
                    httpRequestResponse(httpRequestNewP(client, STRDEF("GET"), STRDEF("/")), false), FormatError,
                    "response status '200OK' must have a space after the status code");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("unexpected end of headers");

                hrnServerScriptAccept(http);

                hrnServerScriptExpectZ(http, "GET / HTTP/1.1\r\n" TEST_USER_AGENT "\r\n");
                hrnServerScriptReplyZ(http, "HTTP/1.1 200 OK\r\n");

                hrnServerScriptClose(http);

                TEST_ERROR(
                    httpRequestResponse(httpRequestNewP(client, STRDEF("GET"), STRDEF("/")), false), FileReadError,
                    "unexpected eof while reading line");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("missing colon in header");

                hrnServerScriptAccept(http);

                hrnServerScriptExpectZ(http, "GET / HTTP/1.1\r\n" TEST_USER_AGENT "\r\n");
                hrnServerScriptReplyZ(http, "HTTP/1.1 200 OK\r\nheader-value\r\n");

                hrnServerScriptClose(http);

                TEST_ERROR(
                    httpRequestResponse(httpRequestNewP(client, STRDEF("GET"), STRDEF("/")), false), FormatError,
                    "header 'header-value' missing colon");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("invalid transfer encoding");

                hrnServerScriptAccept(http);

                hrnServerScriptExpectZ(http, "GET / HTTP/1.1\r\n" TEST_USER_AGENT "\r\n");
                hrnServerScriptReplyZ(http, "HTTP/1.1 200 OK\r\ntransfer-encoding:bogus\r\n");

                hrnServerScriptClose(http);

                TEST_ERROR(
                    httpRequestResponse(httpRequestNewP(client, STRDEF("GET"), STRDEF("/")), false), FormatError,
                    "only 'chunked' is supported for 'transfer-encoding' header");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("content length and transfer encoding both set");

                hrnServerScriptAccept(http);

                hrnServerScriptExpectZ(http, "GET / HTTP/1.1\r\n" TEST_USER_AGENT "\r\n");
                hrnServerScriptReplyZ(http, "HTTP/1.1 200 OK\r\ntransfer-encoding:chunked\r\ncontent-length:777\r\n\r\n");

                hrnServerScriptClose(http);

                TEST_ERROR(
                    httpRequestResponse(httpRequestNewP(client, STRDEF("GET"), STRDEF("/")), false), FormatError,
                    "'transfer-encoding' and 'content-length' headers are both set");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("5xx error with no retry");

                hrnServerScriptAccept(http);

                hrnServerScriptExpectZ(http, "GET / HTTP/1.1\r\n" TEST_USER_AGENT "\r\n");
                hrnServerScriptReplyZ(http, "HTTP/1.1 503 Slow Down\r\n\r\n");

                hrnServerScriptClose(http);

                TEST_ERROR(
                    httpRequestResponse(httpRequestNewP(client, STRDEF("GET"), STRDEF("/")), false), ServiceError,
                    "[503] Slow Down");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("request with no content (with an internal error)");

                hrnServerScriptAccept(http);

                hrnServerScriptExpectZ(http,
                    "GET /?name=%2Fpath%2FA%20Z.txt&type=test HTTP/1.1\r\n" TEST_USER_AGENT "host:myhost.com\r\n\r\n");
                hrnServerScriptReplyZ(http, "HTTP/1.1 500 Internal Error\r\nConnection:close\r\n\r\n");

                hrnServerScriptClose(http);
                hrnServerScriptAccept(http);

                hrnServerScriptExpectZ(
                    http, "GET /?name=%2Fpath%2FA%20Z.txt&type=test HTTP/1.1\r\n" TEST_USER_AGENT "host:myhost.com\r\n\r\n");
                hrnServerScriptReplyZ(
                    http, "HTTP/1.1 200 OK\r\nkey1:0\r\n key2 : value2\r\nConnection:ack\r\ncontent-length:0\r\n\r\n");

                HttpHeader *headerRequest = httpHeaderNew(NULL);
                httpHeaderAdd(headerRequest, STRDEF("host"), STRDEF("myhost.com"));

                HttpQuery *query = httpQueryNewP();
                httpQueryAdd(query, STRDEF("name"), STRDEF("/path/A Z.txt"));
                httpQueryAdd(query, STRDEF("type"), STRDEF("test"));

                client->pub.timeout = 5000;

                HttpRequest *request = NULL;
                HttpResponse *response = NULL;

                MEM_CONTEXT_TEMP_BEGIN()
                {
                    TEST_ASSIGN(
                        request, httpRequestNewP(client, STRDEF("GET"), STRDEF("/"), .query = query, .header = headerRequest),
                        "request");
                    TEST_ASSIGN(response, httpRequestResponse(request, false), "request");

                    TEST_RESULT_VOID(httpRequestMove(request, memContextPrior()), "move request");
                    TEST_RESULT_VOID(httpResponseMove(response, memContextPrior()), "move response");
                }
                MEM_CONTEXT_TEMP_END();

                TEST_RESULT_STR_Z(httpRequestVerb(request), "GET", "check request verb");
                TEST_RESULT_STR_Z(httpRequestPath(request), "/", "check request path");
                TEST_RESULT_STR_Z(
                    httpQueryRenderP(httpRequestQuery(request)), "name=%2Fpath%2FA%20Z.txt&type=test", "check request query");
                TEST_RESULT_PTR_NE(httpRequestHeader(request), NULL, "check request headers");

                TEST_RESULT_UINT(httpResponseCode(response), 200, "check response code");
                TEST_RESULT_BOOL(httpResponseCodeOk(response), true, "check response code ok");
                TEST_RESULT_STR_Z(httpResponseReason(response), "OK", "check response message");
                TEST_RESULT_UINT(httpResponseEof(response), true, "io is eof");
                TEST_RESULT_STR_Z(
                    httpHeaderToLog(httpResponseHeader(response)),
                    "{connection: 'ack', content-length: '0', key1: '0', key2: 'value2'}", "check response headers");
                TEST_RESULT_UINT(bufSize(httpResponseContent(response)), 0, "content is empty");

                TEST_RESULT_VOID(httpResponseFree(response), "free response");
                TEST_RESULT_VOID(httpRequestFree(request), "free request");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("head request with content-length but no content");

                hrnServerScriptExpectZ(http, "HEAD / HTTP/1.1\r\n" TEST_USER_AGENT "\r\n");
                hrnServerScriptReplyZ(http, "HTTP/1.0 200 OK\r\ncontent-length:380\r\n\r\n");

                hrnServerScriptClose(http);

                TEST_ASSIGN(response, httpRequestResponse(httpRequestNewP(client, STRDEF("HEAD"), STRDEF("/")), true), "request");
                TEST_RESULT_UINT(httpResponseCode(response), 200, "check response code");
                TEST_RESULT_STR_Z(httpResponseReason(response), "OK", "check response message");
                TEST_RESULT_BOOL(httpResponseEof(response), true, "io is eof");
                TEST_RESULT_PTR(response->session, NULL, "session is not busy");
                TEST_RESULT_STR_Z(
                    httpHeaderToLog(httpResponseHeader(response)),  "{content-length: '380'}", "check response headers");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("head request with transfer encoding but no content");

                hrnServerScriptAccept(http);

                hrnServerScriptExpectZ(http, "HEAD / HTTP/1.1\r\n" TEST_USER_AGENT "\r\n");
                hrnServerScriptReplyZ(http, "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n");

                TEST_ASSIGN(response, httpRequestResponse(httpRequestNewP(client, STRDEF("HEAD"), STRDEF("/")), true), "request");
                TEST_RESULT_UINT(httpResponseCode(response), 200, "check response code");
                TEST_RESULT_STR_Z(httpResponseReason(response), "OK", "check response message");
                TEST_RESULT_BOOL(httpResponseEof(response), true, "io is eof");
                TEST_RESULT_PTR(response->session, NULL, "session is not busy");
                TEST_RESULT_STR_Z(
                    httpHeaderToLog(httpResponseHeader(response)),  "{transfer-encoding: 'chunked'}", "check response headers");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("head request with connection close but no content");

                hrnServerScriptExpectZ(http, "HEAD / HTTP/1.1\r\n" TEST_USER_AGENT "\r\n");
                hrnServerScriptReplyZ(http, "HTTP/1.1 200 OK\r\nConnection:close\r\n\r\n");

                hrnServerScriptClose(http);

                TEST_ASSIGN(response, httpRequestResponse(httpRequestNewP(client, STRDEF("HEAD"), STRDEF("/")), true), "request");
                TEST_RESULT_UINT(httpResponseCode(response), 200, "check response code");
                TEST_RESULT_STR_Z(httpResponseReason(response), "OK", "check response message");
                TEST_RESULT_BOOL(httpResponseEof(response), true, "io is eof");
                TEST_RESULT_PTR(response->session, NULL, "session is not busy");
                TEST_RESULT_STR_Z(
                    httpHeaderToLog(httpResponseHeader(response)),  "{connection: 'close'}", "check response headers");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("error with content (with a few slow down errors)");

                hrnServerScriptAccept(http);

                hrnServerScriptExpectZ(http, "GET / HTTP/1.1\r\n" TEST_USER_AGENT "\r\n");
                hrnServerScriptReplyZ(http, "HTTP/1.1 503 Slow Down\r\ncontent-length:3\r\nConnection:close\r\n\r\n123");

                hrnServerScriptClose(http);
                hrnServerScriptAccept(http);

                hrnServerScriptExpectZ(http, "GET / HTTP/1.1\r\n" TEST_USER_AGENT "\r\n");
                hrnServerScriptReplyZ(
                    http, "HTTP/1.1 503 Slow Down\r\nTransfer-Encoding:chunked\r\nConnection:close\r\n\r\n0\r\n\r\n");

                hrnServerScriptClose(http);
                hrnServerScriptAccept(http);

                hrnServerScriptExpectZ(http, "GET / HTTP/1.1\r\n" TEST_USER_AGENT "\r\n");
                hrnServerScriptReplyZ(http, "HTTP/1.1 404 Not Found\r\n\r\n");

                TEST_ASSIGN(request, httpRequestNewP(client, STRDEF("GET"), STRDEF("/")), "request");
                TEST_ASSIGN(response, httpRequestResponse(request, false), "response");
                TEST_RESULT_UINT(httpResponseCode(response), 404, "check response code");
                TEST_RESULT_BOOL(httpResponseCodeOk(response), false, "check response code error");
                TEST_RESULT_STR_Z(httpResponseReason(response), "Not Found", "check response message");
                TEST_RESULT_STR_Z(
                    httpHeaderToLog(httpResponseHeader(response)),  "{}", "check response headers");

                TEST_ERROR(
                    httpRequestError(request, response), ProtocolError,
                    "HTTP request failed with 404 (Not Found):\n"
                    "*** Path/Query ***:\n"
                    "/");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("error with content");

                hrnServerScriptExpectZ(http, "GET /?a=b HTTP/1.1\r\n" TEST_USER_AGENT "hdr1:1\r\nhdr2:2\r\n\r\n");
                hrnServerScriptReplyZ(http, "HTTP/1.1 403 \r\ncontent-length:7\r\n\r\nCONTENT");

                StringList *headerRedact = strLstNew();
                strLstAdd(headerRedact, STRDEF("hdr2"));
                headerRequest = httpHeaderNew(headerRedact);
                httpHeaderAdd(headerRequest, STRDEF("hdr1"), STRDEF("1"));
                httpHeaderAdd(headerRequest, STRDEF("hdr2"), STRDEF("2"));

                TEST_ASSIGN(
                    request,
                    httpRequestNewP(
                        client, STRDEF("GET"), STRDEF("/"), .query = httpQueryAdd(httpQueryNewP(), STRDEF("a"), STRDEF("b")),
                        .header = headerRequest),
                    "request");
                TEST_ASSIGN(response, httpRequestResponse(request, false), "response");
                TEST_RESULT_UINT(httpResponseCode(response), 403, "check response code");
                TEST_RESULT_STR_Z(httpResponseReason(response), "", "check empty response message");
                TEST_RESULT_STR_Z(
                    httpHeaderToLog(httpResponseHeader(response)),  "{content-length: '7'}", "check response headers");
                TEST_RESULT_STR_Z(strNewBuf(httpResponseContent(response)),  "CONTENT", "check response content");

                TEST_ERROR(
                    httpRequestError(request, response), ProtocolError,
                    "HTTP request failed with 403:\n"
                    "*** Path/Query ***:\n"
                    "/?a=b\n"
                    "*** Request Headers ***:\n"
                    "hdr1: 1\n"
                    "hdr2: <redacted>\n"
                    "*** Response Headers ***:\n"
                    "content-length: 7\n"
                    "*** Response Content ***:\n"
                    "CONTENT");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("request with content using content-length");

                hrnServerScriptExpectZ(
                    http, "GET /path/file%201.txt HTTP/1.1\r\n" TEST_USER_AGENT "content-length:30\r\n\r\n"
                        "012345678901234567890123456789");
                hrnServerScriptReplyZ(http, "HTTP/1.1 200 OK\r\nConnection:close\r\n\r\n01234567890123456789012345678901");

                hrnServerScriptClose(http);

                ioBufferSizeSet(30);

                TEST_ASSIGN(
                    response,
                    httpRequestResponse(
                        httpRequestNewP(
                            client, STRDEF("GET"), httpUriEncode(STRDEF("/path/file 1.txt"), true),
                            .header = httpHeaderAdd(httpHeaderNew(NULL), STRDEF("content-length"), STRDEF("30")),
                            .content = BUFSTRDEF("012345678901234567890123456789")), true),
                    "request");
                TEST_RESULT_STR_Z(
                    httpHeaderToLog(httpResponseHeader(response)),  "{connection: 'close'}", "check response headers");
                TEST_RESULT_STR_Z(strNewBuf(httpResponseContent(response)),  "01234567890123456789012345678901", "check response");
                TEST_RESULT_UINT(httpResponseRead(response, bufNew(1), true), 0, "call internal read to check eof");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("request with eof before content complete with retry");

                hrnServerScriptAccept(http);

                hrnServerScriptExpectZ(http, "GET /path/file%201.txt HTTP/1.1\r\n" TEST_USER_AGENT "\r\n");
                hrnServerScriptReplyZ(http, "HTTP/1.1 200 OK\r\ncontent-length:32\r\n\r\n0123456789012345678901234567890");

                hrnServerScriptClose(http);
                hrnServerScriptAccept(http);

                hrnServerScriptExpectZ(http, "GET /path/file%201.txt HTTP/1.1\r\n" TEST_USER_AGENT "\r\n");
                hrnServerScriptReplyZ(http, "HTTP/1.1 200 OK\r\ncontent-length:32\r\n\r\n01234567890123456789012345678901");

                TEST_ASSIGN(
                    response,
                    httpRequestResponse(
                        httpRequestNewP(client, STRDEF("GET"), httpUriEncode(STRDEF("/path/file 1.txt"), true)), true),
                    "request");
                TEST_RESULT_STR_Z(strNewBuf(httpResponseContent(response)),  "01234567890123456789012345678901", "check response");
                TEST_RESULT_UINT(httpResponseRead(response, bufNew(1), true), 0, "call internal read to check eof");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("request with eof before content complete");

                hrnServerScriptExpectZ(http, "GET /path/file%201.txt HTTP/1.1\r\n" TEST_USER_AGENT "\r\n");
                hrnServerScriptReplyZ(http, "HTTP/1.1 200 OK\r\ncontent-length:32\r\n\r\n0123456789012345678901234567890");

                hrnServerScriptClose(http);

                TEST_ASSIGN(
                    response,
                    httpRequestResponse(
                        httpRequestNewP(client, STRDEF("GET"), httpUriEncode(STRDEF("/path/file 1.txt"), true)), false),
                    "request");
                TEST_RESULT_PTR_NE(response->session, NULL, "session is busy");
                TEST_ERROR(ioRead(httpResponseIoRead(response), bufNew(32)), FileReadError, "unexpected EOF reading HTTP content");
                TEST_RESULT_PTR_NE(response->session, NULL, "session is still busy");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("request with chunked content");

                hrnServerScriptAccept(http);

                hrnServerScriptExpectZ(http, "GET / HTTP/1.1\r\n" TEST_USER_AGENT "\r\n");
                hrnServerScriptReplyZ(
                    http,
                    "HTTP/1.1 200 OK\r\nTransfer-Encoding:chunked\r\n\r\n"
                    "20\r\n01234567890123456789012345678901\r\n"
                    "10\r\n0123456789012345\r\n"
                    "0\r\n\r\n");

                TEST_ASSIGN(response, httpRequestResponse(httpRequestNewP(client, STRDEF("GET"), STRDEF("/")), false), "request");
                TEST_RESULT_STR_Z(
                    httpHeaderToLog(httpResponseHeader(response)),  "{transfer-encoding: 'chunked'}", "check response headers");

                Buffer *buffer = bufNew(35);

                TEST_RESULT_VOID(ioRead(httpResponseIoRead(response), buffer),  "read response");
                TEST_RESULT_STR_Z(strNewBuf(buffer),  "01234567890123456789012345678901012", "check response");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("close connection and end server process");

                hrnServerScriptClose(http);
                hrnServerScriptEnd(http);
            }
            HRN_FORK_PARENT_END();
        }
        HRN_FORK_END();

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("statistics exist");

        TEST_RESULT_BOOL(varLstEmpty(kvKeyList(statToKv())), false, "check");
    }

    FUNCTION_HARNESS_RETURN_VOID();
}
