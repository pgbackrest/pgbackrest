/***********************************************************************************************************************************
Test Tls Client
***********************************************************************************************************************************/
#include <unistd.h>

#include "common/time.h"

#include "common/harnessTls.h"

/***********************************************************************************************************************************
Path and prefix for test certificates
***********************************************************************************************************************************/
#define TEST_CERTIFICATE_PREFIX                                     "test/certificate/pgbackrest-test"

/***********************************************************************************************************************************
Test server with subject alternate names
***********************************************************************************************************************************/
static void
testTlsServerAltName(void)
{
    if (fork() == 0)
    {
        harnessTlsServerInit(
            TLS_TEST_PORT,
            strPtr(strNewFmt("%s/" TEST_CERTIFICATE_PREFIX "-alt-name.crt", testRepoPath())),
            strPtr(strNewFmt("%s/" TEST_CERTIFICATE_PREFIX ".key", testRepoPath())));

        // Certificate error on invalid ca path
        harnessTlsServerAccept();
        harnessTlsServerClose();

        // Success on valid ca file and match common name
        harnessTlsServerAccept();
        harnessTlsServerClose();

        // Success on valid ca file and match alt name
        harnessTlsServerAccept();
        harnessTlsServerClose();

        // Unable to find matching hostname in certificate
        harnessTlsServerAccept();
        harnessTlsServerClose();

        // Certificate error
        harnessTlsServerAccept();
        harnessTlsServerClose();

        // Certificate ignored
        harnessTlsServerAccept();
        harnessTlsServerClose();

        exit(0);
    }
}

/***********************************************************************************************************************************
Test server
***********************************************************************************************************************************/
static void
testTlsServer(void)
{
    if (fork() == 0)
    {
        harnessTlsServerInit(TLS_TEST_PORT, TLS_CERT_TEST_CERT, TLS_CERT_TEST_KEY);

        // First protocol exchange
        harnessTlsServerAccept();

        harnessTlsServerExpect("some protocol info");
        harnessTlsServerReply("something:0\nsome content");

        // This will cause the client to disconnect
        sleepMSec(500);

        // Second protocol exchange
        harnessTlsServerExpect("more protocol info");
        harnessTlsServerReply("0123456789AB");

        harnessTlsServerClose();

        exit(0);
    }
}

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    // Additional coverage not provided by testing with actual certificates
    // *****************************************************************************************************************************
    if (testBegin("asn1ToStr() and tlsClientHostVerifyName()"))
    {
        TEST_ERROR(asn1ToStr(NULL), CryptoError, "TLS certificate name entry is missing");

        TEST_ERROR(
            tlsClientHostVerifyName(
                strNew("host"), strNewN("ab\0cd", 5)), CryptoError, "TLS certificate name contains embedded null");

        TEST_RESULT_BOOL(tlsClientHostVerifyName(strNew("host"), strNew("**")), false, "invalid pattern");
        TEST_RESULT_BOOL(tlsClientHostVerifyName(strNew("host"), strNew("*.")), false, "invalid pattern");
        TEST_RESULT_BOOL(tlsClientHostVerifyName(strNew("a.bogus.host.com"), strNew("*.host.com")), false, "invalid host");
    }

    // *****************************************************************************************************************************
    if (testBegin("TlsClient verification"))
    {
        TlsClient *client = NULL;

        // Connection errors
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_ASSIGN(client, tlsClientNew(strNew("99.99.99.99.99"), 9443, 0, true, NULL, NULL), "new client");
        TEST_ERROR(tlsClientOpen(client), FileOpenError, "unable to resolve host '99.99.99.99.99'");

        TEST_ASSIGN(client, tlsClientNew(strNew("localhost"), 9443, 100, true, NULL, NULL), "new client");
        TEST_ERROR(tlsClientOpen(client), FileOpenError, "unable to connect to 'localhost:9443': [111] Connection refused");

        // Certificate location and validation errors
        // -------------------------------------------------------------------------------------------------------------------------
        // Add test hosts
        if (system(                                                                                 // {uncovered - test code}
                "echo \"127.0.0.1 test.pgbackrest.org host.test2.pgbackrest.org test3.pgbackrest.org\" |"
                    " sudo tee -a /etc/hosts > /dev/null") != 0)
        {
            THROW(AssertError, "unable to add test hosts to /etc/hosts");                           // {uncovered+}
        }

        // Start server to test various certificate errors
        testTlsServerAltName();

        TEST_ERROR(
            tlsClientOpen(tlsClientNew(strNew("localhost"), 9443, 500, true, strNew("bogus.crt"), strNew("/bogus"))),
            CryptoError, "unable to set user-defined CA certificate location: [33558530] No such file or directory");
        TEST_ERROR(
            tlsClientOpen(tlsClientNew(strNew("localhost"), 9443, 500, true, NULL, strNew("/bogus"))),
            CryptoError, "unable to verify certificate presented by 'localhost:9443': [20] unable to get local issuer certificate");

        TEST_RESULT_VOID(
            tlsClientOpen(
                tlsClientNew(strNew("test.pgbackrest.org"), 9443, 500, true,
                strNewFmt("%s/" TEST_CERTIFICATE_PREFIX "-ca.crt", testRepoPath()), NULL)),
            "success on valid ca file and match common name");
        TEST_RESULT_VOID(
            tlsClientOpen(
                tlsClientNew(strNew("host.test2.pgbackrest.org"), 9443, 500, true,
                strNewFmt("%s/" TEST_CERTIFICATE_PREFIX "-ca.crt", testRepoPath()), NULL)),
            "success on valid ca file and match alt name");
        TEST_ERROR(
            tlsClientOpen(
                tlsClientNew(strNew("test3.pgbackrest.org"), 9443, 500, true,
                strNewFmt("%s/" TEST_CERTIFICATE_PREFIX "-ca.crt", testRepoPath()), NULL)),
            CryptoError, "unable to find matching hostname in certificate");

        TEST_ERROR(
            tlsClientOpen(
                tlsClientNew(strNew("localhost"), 9443, 500, true, strNewFmt("%s/" TEST_CERTIFICATE_PREFIX ".crt", testRepoPath()),
                NULL)),
            CryptoError, "unable to verify certificate presented by 'localhost:9443': [20] unable to get local issuer certificate");

        TEST_RESULT_VOID(
            tlsClientOpen(tlsClientNew(strNew("localhost"), 9443, 500, false, NULL, NULL)), "success on no verify");
    }
    // *****************************************************************************************************************************
    if (testBegin("TlsClient general usage"))
    {
        TlsClient *client = NULL;

        testTlsServer();
        ioBufferSizeSet(12);

        TEST_ASSIGN(client, tlsClientNew(strNew(TLS_TEST_HOST), 9443, 500, true, NULL, NULL), "new client");
        TEST_RESULT_VOID(tlsClientOpen(client), "open client");

        Buffer *input = bufNewStr(strNew("some protocol info"));
        TEST_RESULT_VOID(ioWrite(tlsClientIoWrite(client), input), "write input");
        ioWriteFlush(tlsClientIoWrite(client));

        Buffer *output = bufNew(12);
        TEST_RESULT_INT(ioRead(tlsClientIoRead(client), output), 12, "read output");
        TEST_RESULT_STR(strPtr(strNewBuf(output)), "something:0\n", "    check output");
        TEST_RESULT_BOOL(ioReadEof(tlsClientIoRead(client)), false, "    check eof = false");

        output = bufNew(12);
        TEST_RESULT_INT(ioRead(tlsClientIoRead(client), output), 12, "read output");
        TEST_RESULT_STR(strPtr(strNewBuf(output)), "some content", "    check output");
        TEST_RESULT_BOOL(ioReadEof(tlsClientIoRead(client)), false, "    check eof = false");

        output = bufNew(12);
        TEST_ERROR(
            ioRead(tlsClientIoRead(client), output), FileReadError,
            "unable to read data from 'tls.test.pgbackrest.org:9443' after 500ms");

        // -------------------------------------------------------------------------------------------------------------------------
        input = bufNewStr(strNew("more protocol info"));
        TEST_RESULT_VOID(tlsClientOpen(client), "open client again (it is already open)");
        TEST_RESULT_VOID(ioWrite(tlsClientIoWrite(client), input), "write input");
        ioWriteFlush(tlsClientIoWrite(client));

        output = bufNew(12);
        TEST_RESULT_INT(ioRead(tlsClientIoRead(client), output), 12, "read output");
        TEST_RESULT_STR(strPtr(strNewBuf(output)), "0123456789AB", "    check output");
        TEST_RESULT_BOOL(ioReadEof(tlsClientIoRead(client)), false, "    check eof = false");

        output = bufNew(12);
        TEST_RESULT_INT(ioRead(tlsClientIoRead(client), output), 0, "read no output after eof");
        TEST_RESULT_BOOL(ioReadEof(tlsClientIoRead(client)), true, "    check eof = true");

        TEST_RESULT_VOID(tlsClientFree(client), "free client");
        TEST_RESULT_VOID(tlsClientFree(NULL), "free null client");
    }

    FUNCTION_HARNESS_RESULT_VOID();
}
