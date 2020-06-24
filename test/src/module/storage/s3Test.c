/***********************************************************************************************************************************
Test S3 Storage
***********************************************************************************************************************************/
#include <unistd.h>

#include "common/io/handleRead.h"
#include "common/io/handleWrite.h"

#include "common/harnessConfig.h"
#include "common/harnessFork.h"
#include "common/harnessStorage.h"
#include "common/harnessTls.h"

/***********************************************************************************************************************************
Constants
***********************************************************************************************************************************/
#define S3_TEST_HOST                                                "s3.amazonaws.com"

/***********************************************************************************************************************************
Helper to build test requests
***********************************************************************************************************************************/
typedef struct TestRequestParam
{
    VAR_PARAM_HEADER;
    const char *content;
} TestRequestParam;

#define testRequestP(s3, verb, uri, ...)                                                                                           \
    testRequest(s3, verb, uri, (TestRequestParam){VAR_PARAM_INIT, __VA_ARGS__})

void
testRequest(Storage *s3, const char *verb, const char *uri, TestRequestParam param)
{
    // Add authorization string
    String *request = strNewFmt(
        "%s %s HTTP/1.1\r\n"
            "authorization:AWS4-HMAC-SHA256 Credential=AKIAIOSFODNN7EXAMPLE/\?\?\?\?\?\?\?\?/us-east-1/s3/aws4_request,"
                "SignedHeaders=content-length;",
        verb, uri);

    if (param.content != NULL)
        strCatZ(request, "content-md5;");

    strCatFmt(
        request,
        "host;x-amz-content-sha256;x-amz-date,Signature=????????????????????????????????????????????????????????????????\r\n"
        "content-length:%zu\r\n",
        param.content == NULL ? 0 : strlen(param.content));

    // Add md5
    if (param.content != NULL)
    {
        char md5Hash[HASH_TYPE_MD5_SIZE_HEX];
        encodeToStr(encodeBase64, bufPtr(cryptoHashOne(HASH_TYPE_MD5_STR, BUFSTRZ(param.content))), HASH_TYPE_M5_SIZE, md5Hash);
        strCatFmt(request, "content-md5:%s\r\n", md5Hash);
    }

    // Add host
    if (((StorageS3 *)storageDriver(s3))->uriStyle == storageS3UriStyleHost)
        strCatFmt(request, "host:bucket." S3_TEST_HOST "\r\n");
    else
        strCatFmt(request, "host:" S3_TEST_HOST "\r\n");

    // Add content sha256 and date
    strCatFmt(
        request,
        "x-amz-content-sha256:%s\r\n"
        "x-amz-date:????????T??????Z" "\r\n"
        "\r\n",
        param.content == NULL ? HASH_TYPE_SHA256_ZERO : strPtr(bufHex(cryptoHashOne(HASH_TYPE_SHA256_STR,
        BUFSTRZ(param.content)))));

    // Add content
    if (param.content != NULL)
        strCatZ(request, param.content);

    hrnTlsServerExpect(request);
}

/***********************************************************************************************************************************
Helper to build test responses
***********************************************************************************************************************************/
typedef struct TestResponseParam
{
    VAR_PARAM_HEADER;
    unsigned int code;
    const char *header;
    const char *content;
} TestResponseParam;

#define testResponseP(...)                                                                                                         \
    testResponse((TestResponseParam){VAR_PARAM_INIT, __VA_ARGS__})

void
testResponse(TestResponseParam param)
{
    // Set code to 200 if not specified
    param.code = param.code == 0 ? 200 : param.code;

    // Output header and code
    String *response = strNewFmt("HTTP/1.1 %u ", param.code);

    // Add reason for some codes
    switch (param.code)
    {
        case 200:
        {
            strCatZ(response, "OK");
            break;
        }

        case 403:
        {
            strCatZ(response, "Forbidden");
            break;
        }
    }

    // End header
    strCatZ(response, "\r\n");

    // Headers
    if (param.header != NULL)
        strCatFmt(response, "%s\r\n", param.header);

    // Content
    if (param.content != NULL)
    {
        strCatFmt(
            response,
            "content-length:%zu\r\n"
                "\r\n"
                "%s",
            strlen(param.content), param.content);
    }
    else
        strCatZ(response, "\r\n");

    hrnTlsServerReply(response);
}

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    // Test strings
    const String *path = strNew("/");
    const String *bucket = strNew("bucket");
    const String *region = strNew("us-east-1");
    const String *endPoint = strNew("s3.amazonaws.com");
    const String *host = hrnTlsServerHost();
    const unsigned int port = hrnTlsServerPort();
    const String *accessKey = strNew("AKIAIOSFODNN7EXAMPLE");
    const String *secretAccessKey = strNew("wJalrXUtnFEMI/K7MDENG/bPxRfiCYEXAMPLEKEY");
    const String *securityToken = strNew(
        "AQoDYXdzEPT//////////wEXAMPLEtc764bNrC9SAPBSM22wDOk4x4HIZ8j4FZTwdQWLWsKWHGBuFqwAeMicRXmxfpSPfIeoIYRqTflfKD8YUuwthAx7mSEI/q"
        "kPpKPi/kMcGdQrmGdeehM4IC1NtBmUpp2wUE8phUZampKsburEDy0KPkyQDYwT7WZ0wq5VSXDvp75YU9HFvlRd8Tx6q6fE8YQcHNVXAkiY9q6d+xo0rKwT38xV"
        "qr7ZD0u0iPPkUL64lIZbqBAz+scqKmlzm8FDrypNC9Yjc8fPOLn9FX9KSYvKTr4rvx3iSIlTJabIQwj2ICCR/oLxBA==");

    // *****************************************************************************************************************************
    if (testBegin("storageS3New() and storageRepoGet()"))
    {
        // Only required options
        // -------------------------------------------------------------------------------------------------------------------------
        StringList *argList = strLstNew();
        strLstAddZ(argList, "--stanza=db");
        strLstAddZ(argList, "--" CFGOPT_PG1_PATH "=/path/to/pg");
        strLstAddZ(argList, "--repo1-type=s3");
        strLstAdd(argList, strNewFmt("--repo1-path=%s", strPtr(path)));
        strLstAdd(argList, strNewFmt("--repo1-s3-bucket=%s", strPtr(bucket)));
        strLstAdd(argList, strNewFmt("--repo1-s3-region=%s", strPtr(region)));
        strLstAdd(argList, strNewFmt("--repo1-s3-endpoint=%s", strPtr(endPoint)));
        setenv("PGBACKREST_REPO1_S3_KEY", strPtr(accessKey), true);
        setenv("PGBACKREST_REPO1_S3_KEY_SECRET", strPtr(secretAccessKey), true);
        harnessCfgLoad(cfgCmdArchiveGet, argList);

        Storage *storage = NULL;
        TEST_ASSIGN(storage, storageRepoGet(strNew(STORAGE_TYPE_S3), false), "get S3 repo storage");
        TEST_RESULT_STR(storage->path, path, "    check path");
        TEST_RESULT_STR(((StorageS3 *)storage->driver)->bucket, bucket, "    check bucket");
        TEST_RESULT_STR(((StorageS3 *)storage->driver)->region, region, "    check region");
        TEST_RESULT_STR(
            ((StorageS3 *)storage->driver)->bucketEndpoint, strNewFmt("%s.%s", strPtr(bucket), strPtr(endPoint)), "    check host");
        TEST_RESULT_STR(((StorageS3 *)storage->driver)->accessKey, accessKey, "    check access key");
        TEST_RESULT_STR(((StorageS3 *)storage->driver)->secretAccessKey, secretAccessKey, "    check secret access key");
        TEST_RESULT_STR(((StorageS3 *)storage->driver)->securityToken, NULL, "    check security token");
        TEST_RESULT_BOOL(storageFeature(storage, storageFeaturePath), false, "    check path feature");
        TEST_RESULT_BOOL(storageFeature(storage, storageFeatureCompress), false, "    check compress feature");

        // Add default options
        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAddZ(argList, "--stanza=db");
        strLstAddZ(argList, "--" CFGOPT_PG1_PATH "=/path/to/pg");
        strLstAddZ(argList, "--repo1-type=s3");
        strLstAdd(argList, strNewFmt("--repo1-path=%s", strPtr(path)));
        strLstAdd(argList, strNewFmt("--repo1-s3-bucket=%s", strPtr(bucket)));
        strLstAdd(argList, strNewFmt("--repo1-s3-region=%s", strPtr(region)));
        strLstAdd(argList, strNewFmt("--repo1-s3-endpoint=%s", strPtr(endPoint)));
        strLstAdd(argList, strNewFmt("--repo1-s3-host=%s", strPtr(host)));
#ifdef TEST_CONTAINER_REQUIRED
        strLstAddZ(argList, "--repo1-s3-ca-path=" TLS_CERT_FAKE_PATH);
        strLstAddZ(argList, "--repo1-s3-ca-file=" TLS_CERT_TEST_CERT);
#endif
        setenv("PGBACKREST_REPO1_S3_KEY", strPtr(accessKey), true);
        setenv("PGBACKREST_REPO1_S3_KEY_SECRET", strPtr(secretAccessKey), true);
        setenv("PGBACKREST_REPO1_S3_TOKEN", strPtr(securityToken), true);
        harnessCfgLoad(cfgCmdArchiveGet, argList);

        TEST_ASSIGN(storage, storageRepoGet(strNew(STORAGE_TYPE_S3), false), "get S3 repo storage with options");
        TEST_RESULT_STR(((StorageS3 *)storage->driver)->bucket, bucket, "    check bucket");
        TEST_RESULT_STR(((StorageS3 *)storage->driver)->region, region, "    check region");
        TEST_RESULT_STR(
            ((StorageS3 *)storage->driver)->bucketEndpoint, strNewFmt("%s.%s", strPtr(bucket), strPtr(endPoint)), "    check host");
        TEST_RESULT_UINT(((StorageS3 *)storage->driver)->port, 443, "    check port");
        TEST_RESULT_STR(((StorageS3 *)storage->driver)->accessKey, accessKey, "    check access key");
        TEST_RESULT_STR(((StorageS3 *)storage->driver)->secretAccessKey, secretAccessKey, "    check secret access key");
        TEST_RESULT_STR(((StorageS3 *)storage->driver)->securityToken, securityToken, "    check security token");

        // Add a port to the endpoint
        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAddZ(argList, "--stanza=db");
        strLstAddZ(argList, "--" CFGOPT_PG1_PATH "=/path/to/pg");
        strLstAddZ(argList, "--repo1-type=s3");
        strLstAdd(argList, strNewFmt("--repo1-path=%s", strPtr(path)));
        strLstAdd(argList, strNewFmt("--repo1-s3-bucket=%s", strPtr(bucket)));
        strLstAdd(argList, strNewFmt("--repo1-s3-region=%s", strPtr(region)));
        strLstAdd(argList, strNewFmt("--repo1-s3-endpoint=%s:999", strPtr(endPoint)));
        setenv("PGBACKREST_REPO1_S3_KEY", strPtr(accessKey), true);
        setenv("PGBACKREST_REPO1_S3_KEY_SECRET", strPtr(secretAccessKey), true);
        setenv("PGBACKREST_REPO1_S3_TOKEN", strPtr(securityToken), true);
        harnessCfgLoad(cfgCmdArchiveGet, argList);

        TEST_ASSIGN(storage, storageRepoGet(strNew(STORAGE_TYPE_S3), false), "get S3 repo storage with options");
        TEST_RESULT_STR(((StorageS3 *)storage->driver)->bucket, bucket, "    check bucket");
        TEST_RESULT_STR(((StorageS3 *)storage->driver)->region, region, "    check region");
        TEST_RESULT_STR(
            ((StorageS3 *)storage->driver)->bucketEndpoint, strNewFmt("%s.%s", strPtr(bucket), strPtr(endPoint)), "    check host");
        TEST_RESULT_UINT(((StorageS3 *)storage->driver)->port, 999, "    check port");
        TEST_RESULT_STR(((StorageS3 *)storage->driver)->accessKey, accessKey, "    check access key");
        TEST_RESULT_STR(((StorageS3 *)storage->driver)->secretAccessKey, secretAccessKey, "    check secret access key");
        TEST_RESULT_STR(((StorageS3 *)storage->driver)->securityToken, securityToken, "    check security token");

        // Also add port to the host
        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAddZ(argList, "--stanza=db");
        strLstAddZ(argList, "--" CFGOPT_PG1_PATH "=/path/to/pg");
        strLstAddZ(argList, "--repo1-type=s3");
        strLstAdd(argList, strNewFmt("--repo1-path=%s", strPtr(path)));
        strLstAdd(argList, strNewFmt("--repo1-s3-bucket=%s", strPtr(bucket)));
        strLstAdd(argList, strNewFmt("--repo1-s3-region=%s", strPtr(region)));
        strLstAdd(argList, strNewFmt("--repo1-s3-endpoint=%s:999", strPtr(endPoint)));
        strLstAdd(argList, strNewFmt("--repo1-s3-host=%s:7777", strPtr(host)));
        setenv("PGBACKREST_REPO1_S3_KEY", strPtr(accessKey), true);
        setenv("PGBACKREST_REPO1_S3_KEY_SECRET", strPtr(secretAccessKey), true);
        setenv("PGBACKREST_REPO1_S3_TOKEN", strPtr(securityToken), true);
        harnessCfgLoad(cfgCmdArchiveGet, argList);

        TEST_ASSIGN(storage, storageRepoGet(strNew(STORAGE_TYPE_S3), false), "get S3 repo storage with options");
        TEST_RESULT_STR(((StorageS3 *)storage->driver)->bucket, bucket, "    check bucket");
        TEST_RESULT_STR(((StorageS3 *)storage->driver)->region, region, "    check region");
        TEST_RESULT_STR(
            ((StorageS3 *)storage->driver)->bucketEndpoint, strNewFmt("%s.%s", strPtr(bucket), strPtr(endPoint)), "    check host");
        TEST_RESULT_UINT(((StorageS3 *)storage->driver)->port, 7777, "    check port");
        TEST_RESULT_STR(((StorageS3 *)storage->driver)->accessKey, accessKey, "    check access key");
        TEST_RESULT_STR(((StorageS3 *)storage->driver)->secretAccessKey, secretAccessKey, "    check secret access key");
        TEST_RESULT_STR(((StorageS3 *)storage->driver)->securityToken, securityToken, "    check security token");

        // Use the port option to override both
        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAddZ(argList, "--stanza=db");
        strLstAddZ(argList, "--" CFGOPT_PG1_PATH "=/path/to/pg");
        strLstAddZ(argList, "--repo1-type=s3");
        strLstAdd(argList, strNewFmt("--repo1-path=%s", strPtr(path)));
        strLstAdd(argList, strNewFmt("--repo1-s3-bucket=%s", strPtr(bucket)));
        strLstAdd(argList, strNewFmt("--repo1-s3-region=%s", strPtr(region)));
        strLstAdd(argList, strNewFmt("--repo1-s3-endpoint=%s:999", strPtr(endPoint)));
        strLstAdd(argList, strNewFmt("--repo1-s3-host=%s:7777", strPtr(host)));
        strLstAddZ(argList, "--repo1-s3-port=9001");
        setenv("PGBACKREST_REPO1_S3_KEY", strPtr(accessKey), true);
        setenv("PGBACKREST_REPO1_S3_KEY_SECRET", strPtr(secretAccessKey), true);
        setenv("PGBACKREST_REPO1_S3_TOKEN", strPtr(securityToken), true);
        harnessCfgLoad(cfgCmdArchiveGet, argList);

        TEST_ASSIGN(storage, storageRepoGet(strNew(STORAGE_TYPE_S3), false), "get S3 repo storage with options");
        TEST_RESULT_STR(((StorageS3 *)storage->driver)->bucket, bucket, "    check bucket");
        TEST_RESULT_STR(((StorageS3 *)storage->driver)->region, region, "    check region");
        TEST_RESULT_STR(
            ((StorageS3 *)storage->driver)->bucketEndpoint, strNewFmt("%s.%s", strPtr(bucket), strPtr(endPoint)), "    check host");
        TEST_RESULT_UINT(((StorageS3 *)storage->driver)->port, 9001, "    check port");
        TEST_RESULT_STR(((StorageS3 *)storage->driver)->accessKey, accessKey, "    check access key");
        TEST_RESULT_STR(((StorageS3 *)storage->driver)->secretAccessKey, secretAccessKey, "    check secret access key");
        TEST_RESULT_STR(((StorageS3 *)storage->driver)->securityToken, securityToken, "    check security token");
    }

    // *****************************************************************************************************************************
    if (testBegin("storageS3DateTime() and storageS3Auth()"))
    {
        TEST_RESULT_STR_Z(storageS3DateTime(1491267845), "20170404T010405Z", "static date");

        // -------------------------------------------------------------------------------------------------------------------------
        StorageS3 *driver = (StorageS3 *)storageDriver(
            storageS3New(
                path, true, NULL, bucket, endPoint, storageS3UriStyleHost, region, accessKey, secretAccessKey, NULL, 16, 2, NULL, 0,
                0, testContainer(), NULL, NULL));

        HttpHeader *header = httpHeaderNew(NULL);

        HttpQuery *query = httpQueryNew();
        httpQueryAdd(query, strNew("list-type"), strNew("2"));

        TEST_RESULT_VOID(
            storageS3Auth(driver, strNew("GET"), strNew("/"), query, strNew("20170606T121212Z"), header, HASH_TYPE_SHA256_ZERO_STR),
            "generate authorization");
        TEST_RESULT_STR_Z(
            httpHeaderGet(header, strNew("authorization")),
            "AWS4-HMAC-SHA256 Credential=AKIAIOSFODNN7EXAMPLE/20170606/us-east-1/s3/aws4_request,"
                "SignedHeaders=host;x-amz-content-sha256;x-amz-date,"
                "Signature=cb03bf1d575c1f8904dabf0e573990375340ab293ef7ad18d049fc1338fd89b3",
            "    check authorization header");

        // Test again to be sure cache signing key is used
        const Buffer *lastSigningKey = driver->signingKey;

        TEST_RESULT_VOID(
            storageS3Auth(driver, strNew("GET"), strNew("/"), query, strNew("20170606T121212Z"), header, HASH_TYPE_SHA256_ZERO_STR),
            "generate authorization");
        TEST_RESULT_STR_Z(
            httpHeaderGet(header, strNew("authorization")),
            "AWS4-HMAC-SHA256 Credential=AKIAIOSFODNN7EXAMPLE/20170606/us-east-1/s3/aws4_request,"
                "SignedHeaders=host;x-amz-content-sha256;x-amz-date,"
                "Signature=cb03bf1d575c1f8904dabf0e573990375340ab293ef7ad18d049fc1338fd89b3",
            "    check authorization header");
        TEST_RESULT_BOOL(driver->signingKey == lastSigningKey, true, "    check signing key was reused");

        // Change the date to generate a new signing key
        TEST_RESULT_VOID(
            storageS3Auth(driver, strNew("GET"), strNew("/"), query, strNew("20180814T080808Z"), header, HASH_TYPE_SHA256_ZERO_STR),
            "    generate authorization");
        TEST_RESULT_STR_Z(
            httpHeaderGet(header, strNew("authorization")),
            "AWS4-HMAC-SHA256 Credential=AKIAIOSFODNN7EXAMPLE/20180814/us-east-1/s3/aws4_request,"
                "SignedHeaders=host;x-amz-content-sha256;x-amz-date,"
                "Signature=d0fa9c36426eb94cdbaf287a7872c7a3b6c913f523163d0d7debba0758e36f49",
            "    check authorization header");
        TEST_RESULT_BOOL(driver->signingKey != lastSigningKey, true, "    check signing key was regenerated");

        // Test with security token
        // -------------------------------------------------------------------------------------------------------------------------
        driver = (StorageS3 *)storageDriver(
            storageS3New(
                path, true, NULL, bucket, endPoint, storageS3UriStyleHost, region, accessKey, secretAccessKey, securityToken, 16, 2,
                NULL, 0, 0, testContainer(), NULL, NULL));

        TEST_RESULT_VOID(
            storageS3Auth(driver, strNew("GET"), strNew("/"), query, strNew("20170606T121212Z"), header, HASH_TYPE_SHA256_ZERO_STR),
            "generate authorization");
        TEST_RESULT_STR_Z(
            httpHeaderGet(header, strNew("authorization")),
            "AWS4-HMAC-SHA256 Credential=AKIAIOSFODNN7EXAMPLE/20170606/us-east-1/s3/aws4_request,"
                "SignedHeaders=host;x-amz-content-sha256;x-amz-date;x-amz-security-token,"
                "Signature=c12565bf5d7e0ef623f76d66e09e5431aebef803f6a25a01c586525f17e474a3",
            "    check authorization header");
    }

    // *****************************************************************************************************************************
    if (testBegin("storageS3*(), StorageReadS3, and StorageWriteS3"))
    {
        HARNESS_FORK_BEGIN()
        {
            HARNESS_FORK_CHILD_BEGIN(0, true)
            {
                TEST_RESULT_VOID(
                    hrnTlsServerRun(ioHandleReadNew(strNew("test server read"), HARNESS_FORK_CHILD_READ(), 5000)),
                    "s3 server begin");
            }
            HARNESS_FORK_CHILD_END();

            HARNESS_FORK_PARENT_BEGIN()
            {
                hrnTlsClientBegin(ioHandleWriteNew(strNew("test client write"), HARNESS_FORK_PARENT_WRITE_PROCESS(0)));

                Storage *s3 = storageS3New(
                    path, true, NULL, bucket, endPoint, storageS3UriStyleHost, region, accessKey, secretAccessKey, NULL, 16, 2,
                    host, port, 5000, testContainer(), NULL, NULL);

                // Coverage for noop functions
                // -----------------------------------------------------------------------------------------------------------------
                TEST_RESULT_VOID(storagePathSyncP(s3, strNew("path")), "path sync is a noop");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("ignore missing file");

                hrnTlsServerAccept();
                testRequestP(s3, HTTP_VERB_GET, "/fi%26le.txt");
                testResponseP(.code = 404);

                TEST_RESULT_PTR(storageGetP(storageNewReadP(s3, strNew("fi&le.txt"), .ignoreMissing = true)), NULL, "get file");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("error on missing file");

                testRequestP(s3, HTTP_VERB_GET, "/file.txt");
                testResponseP(.code = 404);

                TEST_ERROR(
                    storageGetP(storageNewReadP(s3, strNew("file.txt"))), FileMissingError,
                    "unable to open '/file.txt': No such file or directory");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("get file");

                testRequestP(s3, HTTP_VERB_GET, "/file.txt");
                testResponseP(.content = "this is a sample file");

                TEST_RESULT_STR_Z(
                    strNewBuf(storageGetP(storageNewReadP(s3, strNew("file.txt")))), "this is a sample file", "get file");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("get zero-length file");

                testRequestP(s3, HTTP_VERB_GET, "/file0.txt");
                testResponseP();

                TEST_RESULT_STR_Z(strNewBuf(storageGetP(storageNewReadP(s3, strNew("file0.txt")))), "", "get zero-length file");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("non-404 error");

                testRequestP(s3, HTTP_VERB_GET, "/file.txt");
                testResponseP(.code = 303, .content = "CONTENT");

                StorageRead *read = NULL;
                TEST_ASSIGN(read, storageNewReadP(s3, strNew("file.txt"), .ignoreMissing = true), "new read file");
                TEST_RESULT_BOOL(storageReadIgnoreMissing(read), true, "    check ignore missing");
                TEST_RESULT_STR_Z(storageReadName(read), "/file.txt", "    check name");

                TEST_ERROR(
                    ioReadOpen(storageReadIo(read)), ProtocolError,
                    "HTTP request failed with 303:\n"
                    "*** URI/Query ***:\n"
                    "/file.txt\n"
                    "*** Request Headers ***:\n"
                    "authorization: <redacted>\n"
                    "content-length: 0\n"
                    "host: bucket." S3_TEST_HOST "\n"
                    "x-amz-content-sha256: e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855\n"
                    "x-amz-date: <redacted>\n"
                    "*** Response Headers ***:\n"
                    "content-length: 7\n"
                    "*** Response Content ***:\n"
                    "CONTENT")

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("write file in one part");

                testRequestP(s3, HTTP_VERB_PUT, "/file.txt", .content = "ABCD");
                testResponseP(
                    .code = 403,
                    .content =
                        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
                        "<Error>"
                        "<Code>RequestTimeTooSkewed</Code>"
                        "<Message>The difference between the request time and the current time is too large.</Message>"
                        "<RequestTime>20190726T221748Z</RequestTime>"
                        "<ServerTime>2019-07-26T22:33:27Z</ServerTime>"
                        "<MaxAllowedSkewMilliseconds>900000</MaxAllowedSkewMilliseconds>"
                        "<RequestId>601AA1A7F7E37AE9</RequestId>"
                        "<HostId>KYMys77PoloZrGCkiQRyOIl0biqdHsk4T2EdTkhzkH1l8x00D4lvv/py5uUuHwQXG9qz6NRuldQ=</HostId>"
                        "</Error>");
                hrnTlsServerClose();
                hrnTlsServerAccept();
                testRequestP(s3, HTTP_VERB_PUT, "/file.txt", .content = "ABCD");
                testResponseP();

                StorageWrite *write = NULL;
                TEST_ASSIGN(write, storageNewWriteP(s3, strNew("file.txt")), "new write");
                TEST_RESULT_VOID(storagePutP(write, BUFSTRDEF("ABCD")), "write");

                TEST_RESULT_BOOL(storageWriteAtomic(write), true, "write is atomic");
                TEST_RESULT_BOOL(storageWriteCreatePath(write), true, "path will be created");
                TEST_RESULT_UINT(storageWriteModeFile(write), 0, "file mode is 0");
                TEST_RESULT_UINT(storageWriteModePath(write), 0, "path mode is 0");
                TEST_RESULT_STR_Z(storageWriteName(write), "/file.txt", "check file name");
                TEST_RESULT_BOOL(storageWriteSyncFile(write), true, "file is synced");
                TEST_RESULT_BOOL(storageWriteSyncPath(write), true, "path is synced");

                TEST_RESULT_VOID(storageWriteS3Close(write->driver), "close file again");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("write zero-length file");

                testRequestP(s3, HTTP_VERB_PUT, "/file.txt", .content = "");
                testResponseP();

                TEST_ASSIGN(write, storageNewWriteP(s3, strNew("file.txt")), "new write");
                TEST_RESULT_VOID(storagePutP(write, NULL), "write");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("write file in chunks with nothing left over on close");

                testRequestP(s3, HTTP_VERB_POST, "/file.txt?uploads=");
                testResponseP(
                    .content =
                        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
                        "<InitiateMultipartUploadResult xmlns=\"http://s3.amazonaws.com/doc/2006-03-01/\">"
                        "<Bucket>bucket</Bucket>"
                        "<Key>file.txt</Key>"
                        "<UploadId>WxRt</UploadId>"
                        "</InitiateMultipartUploadResult>");

                testRequestP(s3, HTTP_VERB_PUT, "/file.txt?partNumber=1&uploadId=WxRt", .content = "1234567890123456");
                testResponseP(.header = "etag:WxRt1");

                testRequestP(s3, HTTP_VERB_PUT, "/file.txt?partNumber=2&uploadId=WxRt", .content = "7890123456789012");
                testResponseP(.header = "eTag:WxRt2");

                testRequestP(
                    s3, HTTP_VERB_POST, "/file.txt?uploadId=WxRt",
                    .content =
                        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                        "<CompleteMultipartUpload>"
                        "<Part><PartNumber>1</PartNumber><ETag>WxRt1</ETag></Part>"
                        "<Part><PartNumber>2</PartNumber><ETag>WxRt2</ETag></Part>"
                        "</CompleteMultipartUpload>\n");
                testResponseP();

                TEST_ASSIGN(write, storageNewWriteP(s3, strNew("file.txt")), "new write");
                TEST_RESULT_VOID(storagePutP(write, BUFSTRDEF("12345678901234567890123456789012")), "write");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("write file in chunks with something left over on close");

                testRequestP(s3, HTTP_VERB_POST, "/file.txt?uploads=");
                testResponseP(
                    .content =
                        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
                        "<InitiateMultipartUploadResult xmlns=\"http://s3.amazonaws.com/doc/2006-03-01/\">"
                        "<Bucket>bucket</Bucket>"
                        "<Key>file.txt</Key>"
                        "<UploadId>RR55</UploadId>"
                        "</InitiateMultipartUploadResult>");

                testRequestP(s3, HTTP_VERB_PUT, "/file.txt?partNumber=1&uploadId=RR55", .content = "1234567890123456");
                testResponseP(.header = "etag:RR551");

                testRequestP(s3, HTTP_VERB_PUT, "/file.txt?partNumber=2&uploadId=RR55", .content = "7890");
                testResponseP(.header = "eTag:RR552");

                testRequestP(
                    s3, HTTP_VERB_POST, "/file.txt?uploadId=RR55",
                    .content =
                        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                        "<CompleteMultipartUpload>"
                        "<Part><PartNumber>1</PartNumber><ETag>RR551</ETag></Part>"
                        "<Part><PartNumber>2</PartNumber><ETag>RR552</ETag></Part>"
                        "</CompleteMultipartUpload>\n");
                testResponseP();

                TEST_ASSIGN(write, storageNewWriteP(s3, strNew("file.txt")), "new write");
                TEST_RESULT_VOID(storagePutP(write, BUFSTRDEF("12345678901234567890")), "write");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("file missing");

                testRequestP(s3, HTTP_VERB_HEAD, "/BOGUS");
                testResponseP(.code = 404);

                TEST_RESULT_BOOL(storageExistsP(s3, strNew("BOGUS")), false, "check");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("info for missing file");

                // File missing
                testRequestP(s3, HTTP_VERB_HEAD, "/BOGUS");
                testResponseP(.code = 404);

                TEST_RESULT_BOOL(storageInfoP(s3, strNew("BOGUS"), .ignoreMissing = true).exists, false, "file does not exist");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("info for file");

                testRequestP(s3, HTTP_VERB_HEAD, "/subdir/file1.txt");
                testResponseP(.header = "content-length:9999\r\nLast-Modified: Wed, 21 Oct 2015 07:28:00 GMT");

                StorageInfo info;
                TEST_ASSIGN(info, storageInfoP(s3, strNew("subdir/file1.txt")), "file exists");
                TEST_RESULT_BOOL(info.exists, true, "    check exists");
                TEST_RESULT_UINT(info.type, storageTypeFile, "    check type");
                TEST_RESULT_UINT(info.size, 9999, "    check exists");
                TEST_RESULT_INT(info.timeModified, 1445412480, "    check time");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("info check existence only");

                testRequestP(s3, HTTP_VERB_HEAD, "/subdir/file2.txt");
                testResponseP(.header = "content-length:777\r\nLast-Modified: Wed, 22 Oct 2015 07:28:00 GMT");

                TEST_ASSIGN(info, storageInfoP(s3, strNew("subdir/file2.txt"), .level = storageInfoLevelExists), "file exists");
                TEST_RESULT_BOOL(info.exists, true, "    check exists");
                TEST_RESULT_UINT(info.type, storageTypeFile, "    check type");
                TEST_RESULT_UINT(info.size, 0, "    check exists");
                TEST_RESULT_INT(info.timeModified, 0, "    check time");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("errorOnMissing invalid because there are no paths");

                TEST_ERROR(
                    storageListP(s3, strNew("/"), .errorOnMissing = true), AssertError,
                    "assertion '!param.errorOnMissing || storageFeature(this, storageFeaturePath)' failed");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("error without xml");

                testRequestP(s3, HTTP_VERB_GET, "/?delimiter=%2F&list-type=2");
                testResponseP(.code = 344);

                TEST_ERROR(storageListP(s3, strNew("/")), ProtocolError,
                    "HTTP request failed with 344:\n"
                    "*** URI/Query ***:\n"
                    "/?delimiter=%2F&list-type=2\n"
                    "*** Request Headers ***:\n"
                    "authorization: <redacted>\n"
                    "content-length: 0\n"
                    "host: bucket." S3_TEST_HOST "\n"
                    "x-amz-content-sha256: e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855\n"
                    "x-amz-date: <redacted>");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("error with xml");

                testRequestP(s3, HTTP_VERB_GET, "/?delimiter=%2F&list-type=2");
                testResponseP(
                    .code = 344,
                    .content =
                        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
                        "<Error>"
                        "<Code>SomeOtherCode</Code>"
                        "</Error>");

                TEST_ERROR(storageListP(s3, strNew("/")), ProtocolError,
                    "HTTP request failed with 344:\n"
                    "*** URI/Query ***:\n"
                    "/?delimiter=%2F&list-type=2\n"
                    "*** Request Headers ***:\n"
                    "authorization: <redacted>\n"
                    "content-length: 0\n"
                    "host: bucket." S3_TEST_HOST "\n"
                    "x-amz-content-sha256: e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855\n"
                    "x-amz-date: <redacted>\n"
                    "*** Response Headers ***:\n"
                    "content-length: 79\n"
                    "*** Response Content ***:\n"
                    "<?xml version=\"1.0\" encoding=\"UTF-8\"?><Error><Code>SomeOtherCode</Code></Error>");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("time skewed error after retries");

                testRequestP(s3, HTTP_VERB_GET, "/?delimiter=%2F&list-type=2");
                testResponseP(
                    .code = 403,
                    .content =
                        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
                        "<Error>"
                        "<Code>RequestTimeTooSkewed</Code>"
                        "<Message>The difference between the request time and the current time is too large.</Message>"
                        "</Error>");

                testRequestP(s3, HTTP_VERB_GET, "/?delimiter=%2F&list-type=2");
                testResponseP(
                    .code = 403,
                    .content =
                        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
                        "<Error>"
                        "<Code>RequestTimeTooSkewed</Code>"
                        "<Message>The difference between the request time and the current time is too large.</Message>"
                        "</Error>");

                testRequestP(s3, HTTP_VERB_GET, "/?delimiter=%2F&list-type=2");
                testResponseP(
                    .code = 403,
                    .content =
                        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
                        "<Error>"
                        "<Code>RequestTimeTooSkewed</Code>"
                        "<Message>The difference between the request time and the current time is too large.</Message>"
                        "</Error>");

                TEST_ERROR(storageListP(s3, strNew("/")), ProtocolError,
                    "HTTP request failed with 403 (Forbidden):\n"
                    "*** URI/Query ***:\n"
                    "/?delimiter=%2F&list-type=2\n"
                    "*** Request Headers ***:\n"
                    "authorization: <redacted>\n"
                    "content-length: 0\n"
                    "host: bucket." S3_TEST_HOST "\n"
                    "x-amz-content-sha256: e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855\n"
                    "x-amz-date: <redacted>\n"
                    "*** Response Headers ***:\n"
                    "content-length: 179\n"
                    "*** Response Content ***:\n"
                    "<?xml version=\"1.0\" encoding=\"UTF-8\"?><Error><Code>RequestTimeTooSkewed</Code>"
                        "<Message>The difference between the request time and the current time is too large.</Message></Error>");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("list basic level");

                testRequestP(s3, HTTP_VERB_GET, "/?delimiter=%2F&list-type=2&prefix=path%2Fto%2F");
                testResponseP(
                    .content =
                        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
                        "<ListBucketResult xmlns=\"http://s3.amazonaws.com/doc/2006-03-01/\">"
                        "    <Contents>"
                        "        <Key>path/to/test_file</Key>"
                        "        <LastModified>2009-10-12T17:50:30.000Z</LastModified>"
                        "        <Size>787</Size>"
                        "    </Contents>"
                        "   <CommonPrefixes>"
                        "       <Prefix>path/to/test_path/</Prefix>"
                        "   </CommonPrefixes>"
                        "</ListBucketResult>");

                HarnessStorageInfoListCallbackData callbackData =
                {
                    .content = strNew(""),
                };

                TEST_ERROR(
                    storageInfoListP(s3, strNew("/"), hrnStorageInfoListCallback, NULL, .errorOnMissing = true),
                    AssertError, "assertion '!param.errorOnMissing || storageFeature(this, storageFeaturePath)' failed");

                TEST_RESULT_VOID(
                    storageInfoListP(s3, strNew("/path/to"), hrnStorageInfoListCallback, &callbackData), "list");
                TEST_RESULT_STR_Z(
                    callbackData.content,
                    "test_path {path}\n"
                    "test_file {file, s=787, t=1255369830}\n",
                    "check");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("list exists level");

                testRequestP(s3, HTTP_VERB_GET, "/?delimiter=%2F&list-type=2");
                testResponseP(
                    .content =
                        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
                        "<ListBucketResult xmlns=\"http://s3.amazonaws.com/doc/2006-03-01/\">"
                        "    <Contents>"
                        "        <Key>test1.txt</Key>"
                        "    </Contents>"
                        "   <CommonPrefixes>"
                        "       <Prefix>path1/</Prefix>"
                        "   </CommonPrefixes>"
                        "</ListBucketResult>");

                callbackData.content = strNew("");

                TEST_RESULT_VOID(
                    storageInfoListP(s3, strNew("/"), hrnStorageInfoListCallback, &callbackData, .level = storageInfoLevelExists),
                    "list");
                TEST_RESULT_STR_Z(
                    callbackData.content,
                    "path1 {}\n"
                    "test1.txt {}\n",
                    "check");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("list a file in root with expression");

                testRequestP(s3, HTTP_VERB_GET, "/?delimiter=%2F&list-type=2&prefix=test");
                testResponseP(
                    .content =
                        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
                        "<ListBucketResult xmlns=\"http://s3.amazonaws.com/doc/2006-03-01/\">"
                        "    <Contents>"
                        "        <Key>test1.txt</Key>"
                        "    </Contents>"
                        "</ListBucketResult>");

                callbackData.content = strNew("");

                TEST_RESULT_VOID(
                    storageInfoListP(
                        s3, strNew("/"), hrnStorageInfoListCallback, &callbackData, .expression = strNew("^test.*$"),
                        .level = storageInfoLevelExists),
                    "list");
                TEST_RESULT_STR_Z(
                    callbackData.content,
                    "test1.txt {}\n",
                    "check");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("list files with continuation");

                testRequestP(s3, HTTP_VERB_GET, "/?delimiter=%2F&list-type=2&prefix=path%2Fto%2F");
                testResponseP(
                    .content =
                        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
                        "<ListBucketResult xmlns=\"http://s3.amazonaws.com/doc/2006-03-01/\">"
                        "    <NextContinuationToken>1ueGcxLPRx1Tr/XYExHnhbYLgveDs2J/wm36Hy4vbOwM=</NextContinuationToken>"
                        "    <Contents>"
                        "        <Key>path/to/test1.txt</Key>"
                        "    </Contents>"
                        "    <Contents>"
                        "        <Key>path/to/test2.txt</Key>"
                        "    </Contents>"
                        "   <CommonPrefixes>"
                        "       <Prefix>path/to/path1/</Prefix>"
                        "   </CommonPrefixes>"
                        "</ListBucketResult>");

                testRequestP(
                    s3, HTTP_VERB_GET,
                    "/?continuation-token=1ueGcxLPRx1Tr%2FXYExHnhbYLgveDs2J%2Fwm36Hy4vbOwM%3D&delimiter=%2F&list-type=2"
                        "&prefix=path%2Fto%2F");
                testResponseP(
                    .content =
                        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
                        "<ListBucketResult xmlns=\"http://s3.amazonaws.com/doc/2006-03-01/\">"
                        "    <Contents>"
                        "        <Key>path/to/test3.txt</Key>"
                        "    </Contents>"
                        "   <CommonPrefixes>"
                        "       <Prefix>path/to/path2/</Prefix>"
                        "   </CommonPrefixes>"
                        "</ListBucketResult>");

                callbackData.content = strNew("");

                TEST_RESULT_VOID(
                    storageInfoListP(
                        s3, strNew("/path/to"), hrnStorageInfoListCallback, &callbackData, .level = storageInfoLevelExists),
                    "list");
                TEST_RESULT_STR_Z(
                    callbackData.content,
                    "path1 {}\n"
                    "test1.txt {}\n"
                    "test2.txt {}\n"
                    "path2 {}\n"
                    "test3.txt {}\n",
                    "check");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("list files with expression");

                testRequestP(s3, HTTP_VERB_GET, "/?delimiter=%2F&list-type=2&prefix=path%2Fto%2Ftest");
                testResponseP(
                    .content =
                        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
                        "<ListBucketResult xmlns=\"http://s3.amazonaws.com/doc/2006-03-01/\">"
                        "    <Contents>"
                        "        <Key>path/to/test1.txt</Key>"
                        "    </Contents>"
                        "    <Contents>"
                        "        <Key>path/to/test2.txt</Key>"
                        "    </Contents>"
                        "    <Contents>"
                        "        <Key>path/to/test3.txt</Key>"
                        "    </Contents>"
                        "   <CommonPrefixes>"
                        "       <Prefix>path/to/test1.path/</Prefix>"
                        "   </CommonPrefixes>"
                        "   <CommonPrefixes>"
                        "       <Prefix>path/to/test2.path/</Prefix>"
                        "   </CommonPrefixes>"
                        "</ListBucketResult>");

                callbackData.content = strNew("");

                TEST_RESULT_VOID(
                    storageInfoListP(
                        s3, strNew("/path/to"), hrnStorageInfoListCallback, &callbackData, .expression = strNew("^test(1|3)"),
                        .level = storageInfoLevelExists),
                    "list");
                TEST_RESULT_STR_Z(
                    callbackData.content,
                    "test1.path {}\n"
                    "test1.txt {}\n"
                    "test3.txt {}\n",
                    "check");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("switch to path-style URIs");

                hrnTlsServerClose();

                s3 = storageS3New(
                    path, true, NULL, bucket, endPoint, storageS3UriStylePath, region, accessKey, secretAccessKey, NULL, 16, 2,
                    host, port, 5000, testContainer(), NULL, NULL);

                hrnTlsServerAccept();

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("error when no recurse because there are no paths");

                TEST_ERROR(
                    storagePathRemoveP(s3, strNew("/")), AssertError,
                    "assertion 'param.recurse || storageFeature(this, storageFeaturePath)' failed");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("remove files from root");

                testRequestP(s3, HTTP_VERB_GET, "/bucket/?list-type=2");
                testResponseP(
                    .content =
                        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
                        "<ListBucketResult xmlns=\"http://s3.amazonaws.com/doc/2006-03-01/\">"
                        "    <Contents>"
                        "        <Key>test1.txt</Key>"
                        "    </Contents>"
                        "    <Contents>"
                        "        <Key>path1/xxx.zzz</Key>"
                        "    </Contents>"
                        "</ListBucketResult>");

                testRequestP(
                    s3, HTTP_VERB_POST, "/bucket/?delete=",
                    .content =
                        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                        "<Delete><Quiet>true</Quiet>"
                        "<Object><Key>test1.txt</Key></Object>"
                        "<Object><Key>path1/xxx.zzz</Key></Object>"
                        "</Delete>\n");
                testResponseP(.content = "<DeleteResult xmlns=\"http://s3.amazonaws.com/doc/2006-03-01/\"></DeleteResult>");

                TEST_RESULT_VOID(storagePathRemoveP(s3, strNew("/"), .recurse = true), "remove");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("remove files in empty subpath (nothing to do)");

                testRequestP(s3, HTTP_VERB_GET, "/bucket/?list-type=2&prefix=path%2F");
                testResponseP(
                    .content =
                        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
                        "<ListBucketResult xmlns=\"http://s3.amazonaws.com/doc/2006-03-01/\">"
                        "</ListBucketResult>");

                TEST_RESULT_VOID(storagePathRemoveP(s3, strNew("/path"), .recurse = true), "remove");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("remove files with continuation");

                testRequestP(s3, HTTP_VERB_GET, "/bucket/?list-type=2&prefix=path%2Fto%2F");
                testResponseP(
                    .content =
                        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
                        "<ListBucketResult xmlns=\"http://s3.amazonaws.com/doc/2006-03-01/\">"
                        "    <NextContinuationToken>continue</NextContinuationToken>"
                        "   <CommonPrefixes>"
                        "       <Prefix>path/to/test3/</Prefix>"
                        "   </CommonPrefixes>"
                        "    <Contents>"
                        "        <Key>path/to/test1.txt</Key>"
                        "    </Contents>"
                        "</ListBucketResult>");

                testRequestP(s3, HTTP_VERB_GET, "/bucket/?continuation-token=continue&list-type=2&prefix=path%2Fto%2F");
                testResponseP(
                    .content =
                        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
                        "<ListBucketResult xmlns=\"http://s3.amazonaws.com/doc/2006-03-01/\">"
                        "    <Contents>"
                        "        <Key>path/to/test3.txt</Key>"
                        "    </Contents>"
                        "    <Contents>"
                        "        <Key>path/to/test2.txt</Key>"
                        "    </Contents>"
                        "</ListBucketResult>");

                testRequestP(
                    s3, HTTP_VERB_POST, "/bucket/?delete=",
                    .content =
                        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                        "<Delete><Quiet>true</Quiet>"
                        "<Object><Key>path/to/test1.txt</Key></Object>"
                        "<Object><Key>path/to/test3.txt</Key></Object>"
                        "</Delete>\n");
                testResponseP();

                testRequestP(
                    s3, HTTP_VERB_POST, "/bucket/?delete=",
                    .content =
                        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                        "<Delete><Quiet>true</Quiet>"
                        "<Object><Key>path/to/test2.txt</Key></Object>"
                        "</Delete>\n");
                testResponseP();

                TEST_RESULT_VOID(storagePathRemoveP(s3, strNew("/path/to"), .recurse = true), "remove");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("remove error");

                testRequestP(s3, HTTP_VERB_GET, "/bucket/?list-type=2&prefix=path%2F");
                testResponseP(
                    .content =
                        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
                        "<ListBucketResult xmlns=\"http://s3.amazonaws.com/doc/2006-03-01/\">"
                        "    <Contents>"
                        "        <Key>path/sample.txt</Key>"
                        "    </Contents>"
                        "    <Contents>"
                        "        <Key>path/sample2.txt</Key>"
                        "    </Contents>"
                        "</ListBucketResult>");

                testRequestP(
                    s3, HTTP_VERB_POST, "/bucket/?delete=",
                    .content =
                        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                        "<Delete><Quiet>true</Quiet>"
                        "<Object><Key>path/sample.txt</Key></Object>"
                        "<Object><Key>path/sample2.txt</Key></Object>"
                        "</Delete>\n");
                testResponseP(
                    .content =
                        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
                        "<DeleteResult xmlns=\"http://s3.amazonaws.com/doc/2006-03-01/\">"
                            "<Error><Key>sample2.txt</Key><Code>AccessDenied</Code><Message>Access Denied</Message></Error>"
                            "</DeleteResult>");

                TEST_ERROR(
                    storagePathRemoveP(s3, strNew("/path"), .recurse = true), FileRemoveError,
                    "unable to remove file 'sample2.txt': [AccessDenied] Access Denied");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("remove file");

                testRequestP(s3, HTTP_VERB_DELETE, "/bucket/path/to/test.txt");
                testResponseP(.code = 204);

                TEST_RESULT_VOID(storageRemoveP(s3, strNew("/path/to/test.txt")), "remove");

                // -----------------------------------------------------------------------------------------------------------------
                hrnTlsClientEnd();
            }
            HARNESS_FORK_PARENT_END();
        }
        HARNESS_FORK_END();
    }

    FUNCTION_HARNESS_RESULT_VOID();
}
