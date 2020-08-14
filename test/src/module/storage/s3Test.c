/***********************************************************************************************************************************
Test S3 Storage
***********************************************************************************************************************************/
#include <unistd.h>

#include "common/io/fdRead.h"
#include "common/io/fdWrite.h"

#include "common/harnessConfig.h"
#include "common/harnessFork.h"
#include "common/harnessServer.h"
#include "common/harnessStorage.h"

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
    const char *accessKey;
    const char *securityToken;
} TestRequestParam;

#define testRequestP(write, s3, verb, uri, ...)                                                                                    \
    testRequest(write, s3, verb, uri, (TestRequestParam){VAR_PARAM_INIT, __VA_ARGS__})

static void
testRequest(IoWrite *write, Storage *s3, const char *verb, const char *uri, TestRequestParam param)
{
    // Get security token from param
    const char *securityToken = param.securityToken == NULL ? NULL : param.securityToken;

    // If s3 storage is set then get the driver
    StorageS3 *driver = NULL;

    if (s3 != NULL)
    {
        driver = (StorageS3 *)storageDriver(s3);

        // Also update the security token if it is not already set
        if (param.securityToken == NULL)
            securityToken = strZNull(driver->securityToken);
    }

    // Add request
    String *request = strNewFmt("%s %s HTTP/1.1\r\n", verb, uri);

    // Add authorization header when s3 service
    if (s3 != NULL)
    {
        strCatFmt(
            request,
                "authorization:AWS4-HMAC-SHA256 Credential=%s/\?\?\?\?\?\?\?\?/us-east-1/s3/aws4_request,"
                    "SignedHeaders=content-length",
                param.accessKey == NULL ? strZ(driver->accessKey) : param.accessKey);

        if (param.content != NULL)
            strCatZ(request, ";content-md5");

        strCatZ(request, ";host;x-amz-content-sha256;x-amz-date");

        if (securityToken != NULL)
            strCatZ(request, ";x-amz-security-token");

        strCatZ(request, ",Signature=????????????????????????????????????????????????????????????????\r\n");
    }

    // Add content-length
    strCatFmt(request, "content-length:%zu\r\n", param.content != NULL ? strlen(param.content) : 0);

    // Add md5
    if (param.content != NULL)
    {
        char md5Hash[HASH_TYPE_MD5_SIZE_HEX];
        encodeToStr(encodeBase64, bufPtr(cryptoHashOne(HASH_TYPE_MD5_STR, BUFSTRZ(param.content))), HASH_TYPE_M5_SIZE, md5Hash);
        strCatFmt(request, "content-md5:%s\r\n", md5Hash);
    }

    // Add host
    if (s3 != NULL)
    {
        if (driver->uriStyle == storageS3UriStyleHost)
        strCatFmt(request, "host:bucket." S3_TEST_HOST "\r\n");
    else
        strCatFmt(request, "host:" S3_TEST_HOST "\r\n");
    }
    else
        strCatFmt(request, "host:%s\r\n", strZ(hrnServerHost()));

    // Add content checksum and date if s3 service
    if (s3 != NULL)
    {
        // Add content sha256 and date
        strCatFmt(
            request,
            "x-amz-content-sha256:%s\r\n"
                "x-amz-date:????????T??????Z" "\r\n",
            param.content == NULL ? HASH_TYPE_SHA256_ZERO : strZ(bufHex(cryptoHashOne(HASH_TYPE_SHA256_STR,
            BUFSTRZ(param.content)))));

        // Add security token
        if (securityToken != NULL)
            strCatFmt(request, "x-amz-security-token:%s\r\n", securityToken);
    }

    // Add final \r\n
    strCatZ(request, "\r\n");

    // Add content
    if (param.content != NULL)
        strCatZ(request, param.content);

    hrnServerScriptExpect(write, request);
}

/***********************************************************************************************************************************
Helper to build test responses
***********************************************************************************************************************************/
typedef struct TestResponseParam
{
    VAR_PARAM_HEADER;
    unsigned int code;
    const char *http;
    const char *header;
    const char *content;
} TestResponseParam;

#define testResponseP(write, ...)                                                                                                  \
    testResponse(write, (TestResponseParam){VAR_PARAM_INIT, __VA_ARGS__})

static void
testResponse(IoWrite *write, TestResponseParam param)
{
    // Set code to 200 if not specified
    param.code = param.code == 0 ? 200 : param.code;

    // Output header and code
    String *response = strNewFmt("HTTP/%s %u ", param.http == NULL ? "1.1" : param.http, param.code);

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

    hrnServerScriptReply(write, response);
}

/***********************************************************************************************************************************
Format ISO-8601 date with - and :
***********************************************************************************************************************************/
static String *
testS3DateTime(time_t time)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(TIME, time);
    FUNCTION_HARNESS_END();

    char buffer[21];

    THROW_ON_SYS_ERROR(
        strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", gmtime(&time)) != sizeof(buffer) - 1, AssertError,
        "unable to format date");

    FUNCTION_HARNESS_RESULT(STRING, strNew(buffer));
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
    const String *host = hrnServerHost();
    const unsigned int port = hrnServerPort(0);
    const unsigned int authPort = hrnServerPort(1);
    const String *accessKey = strNew("AKIAIOSFODNN7EXAMPLE");
    const String *secretAccessKey = strNew("wJalrXUtnFEMI/K7MDENG/bPxRfiCYEXAMPLEKEY");
    const String *securityToken = strNew(
        "AQoDYXdzEPT//////////wEXAMPLEtc764bNrC9SAPBSM22wDOk4x4HIZ8j4FZTwdQWLWsKWHGBuFqwAeMicRXmxfpSPfIeoIYRqTflfKD8YUuwthAx7mSEI/q"
        "kPpKPi/kMcGdQrmGdeehM4IC1NtBmUpp2wUE8phUZampKsburEDy0KPkyQDYwT7WZ0wq5VSXDvp75YU9HFvlRd8Tx6q6fE8YQcHNVXAkiY9q6d+xo0rKwT38xV"
        "qr7ZD0u0iPPkUL64lIZbqBAz+scqKmlzm8FDrypNC9Yjc8fPOLn9FX9KSYvKTr4rvx3iSIlTJabIQwj2ICCR/oLxBA==");
    const String *credRole = STRDEF("credrole");

    // Config settings that are required for every test (without endpoint for special tests)
    StringList *commonArgWithoutEndpointList = strLstNew();
    strLstAddZ(commonArgWithoutEndpointList, "--" CFGOPT_STANZA "=db");
    strLstAddZ(commonArgWithoutEndpointList, "--" CFGOPT_REPO1_TYPE "=s3");
    strLstAdd(commonArgWithoutEndpointList, strNewFmt("--" CFGOPT_REPO1_PATH "=%s", strZ(path)));
    strLstAdd(commonArgWithoutEndpointList, strNewFmt("--" CFGOPT_REPO1_S3_BUCKET "=%s", strZ(bucket)));
    strLstAdd(commonArgWithoutEndpointList, strNewFmt("--" CFGOPT_REPO1_S3_REGION "=%s", strZ(region)));

    // TLS can only be verified in a container
    if (!testContainer())
        strLstAddZ(commonArgWithoutEndpointList, "--no-" CFGOPT_REPO1_S3_VERIFY_TLS);

    // Config settings that are required for every test (with endpoint)
    StringList *commonArgList = strLstDup(commonArgWithoutEndpointList);
    strLstAdd(commonArgList, strNewFmt("--" CFGOPT_REPO1_S3_ENDPOINT "=%s", strZ(endPoint)));

    // *****************************************************************************************************************************
    if (testBegin("storageS3DateTime() and storageS3Auth()"))
    {
        TEST_RESULT_STR_Z(storageS3DateTime(1491267845), "20170404T010405Z", "static date");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("config without token");

        StringList *argList = strLstDup(commonArgList);
        setenv("PGBACKREST_" CFGOPT_REPO1_S3_KEY, strZ(accessKey), true);
        setenv("PGBACKREST_" CFGOPT_REPO1_S3_KEY_SECRET, strZ(secretAccessKey), true);
        harnessCfgLoad(cfgCmdArchivePush, argList);

        StorageS3 *driver = (StorageS3 *)storageDriver(storageRepoGet(STORAGE_S3_TYPE_STR, false));

        TEST_RESULT_STR(driver->bucket, bucket, "check bucket");
        TEST_RESULT_STR(driver->region, region, "check region");
        TEST_RESULT_STR(driver->bucketEndpoint, strNewFmt("%s.%s", strZ(bucket), strZ(endPoint)), "check host");
        TEST_RESULT_STR(driver->accessKey, accessKey, "check access key");
        TEST_RESULT_STR(driver->secretAccessKey, secretAccessKey, "check secret access key");
        TEST_RESULT_STR(driver->securityToken, NULL, "check security token");
        TEST_RESULT_STR(
            httpClientToLog(driver->httpClient),
            strNewFmt(
                "{ioClient: {type: tls, driver: {ioClient: {type: socket, driver: {host: bucket.s3.amazonaws.com, port: 443"
                    ", timeout: 60000}}, timeout: 60000, verifyPeer: %s}}, reusable: 0, timeout: 60000}",
                cvtBoolToConstZ(testContainer())),
            "check http client");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("auth with token");

        HttpHeader *header = httpHeaderNew(NULL);
        HttpQuery *query = httpQueryNewP();
        httpQueryAdd(query, strNew("list-type"), strNew("2"));

        TEST_RESULT_VOID(
            storageS3Auth(driver, strNew("GET"), strNew("/"), query, strNew("20170606T121212Z"), header, HASH_TYPE_SHA256_ZERO_STR),
            "generate authorization");
        TEST_RESULT_STR_Z(
            httpHeaderGet(header, strNew("authorization")),
            "AWS4-HMAC-SHA256 Credential=AKIAIOSFODNN7EXAMPLE/20170606/us-east-1/s3/aws4_request,"
                "SignedHeaders=host;x-amz-content-sha256;x-amz-date,"
                "Signature=cb03bf1d575c1f8904dabf0e573990375340ab293ef7ad18d049fc1338fd89b3",
            "check authorization header");

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
            "check authorization header");
        TEST_RESULT_BOOL(driver->signingKey == lastSigningKey, true, "check signing key was reused");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("change date to generate new signing key");

        TEST_RESULT_VOID(
            storageS3Auth(driver, strNew("GET"), strNew("/"), query, strNew("20180814T080808Z"), header, HASH_TYPE_SHA256_ZERO_STR),
            "generate authorization");
        TEST_RESULT_STR_Z(
            httpHeaderGet(header, strNew("authorization")),
            "AWS4-HMAC-SHA256 Credential=AKIAIOSFODNN7EXAMPLE/20180814/us-east-1/s3/aws4_request,"
                "SignedHeaders=host;x-amz-content-sha256;x-amz-date,"
                "Signature=d0fa9c36426eb94cdbaf287a7872c7a3b6c913f523163d0d7debba0758e36f49",
            "check authorization header");
        TEST_RESULT_BOOL(driver->signingKey != lastSigningKey, true, "check signing key was regenerated");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("config with token, endpoint with custom port, and ca-file/path");

        argList = strLstDup(commonArgWithoutEndpointList);
        strLstAddZ(argList, "--" CFGOPT_REPO1_S3_ENDPOINT "=custom.endpoint:333");
        strLstAddZ(argList, "--" CFGOPT_REPO1_S3_CA_PATH "=/path/to/cert");
        strLstAdd(argList, strNewFmt("--" CFGOPT_REPO1_S3_CA_FILE "=%s/" HRN_SERVER_CERT_PREFIX ".crt", testRepoPath()));
        setenv("PGBACKREST_" CFGOPT_REPO1_S3_KEY, strZ(accessKey), true);
        setenv("PGBACKREST_" CFGOPT_REPO1_S3_KEY_SECRET, strZ(secretAccessKey), true);
        setenv("PGBACKREST_" CFGOPT_REPO1_S3_TOKEN, strZ(securityToken), true);
        harnessCfgLoad(cfgCmdArchivePush, argList);

        driver = (StorageS3 *)storageDriver(storageRepoGet(STORAGE_S3_TYPE_STR, false));

        TEST_RESULT_STR(driver->securityToken, securityToken, "check security token");
        TEST_RESULT_STR(
            httpClientToLog(driver->httpClient),
            strNewFmt(
                "{ioClient: {type: tls, driver: {ioClient: {type: socket, driver: {host: bucket.custom.endpoint, port: 333"
                    ", timeout: 60000}}, timeout: 60000, verifyPeer: %s}}, reusable: 0, timeout: 60000}",
                cvtBoolToConstZ(testContainer())),
            "check http client");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("auth with token");

        TEST_RESULT_VOID(
            storageS3Auth(driver, strNew("GET"), strNew("/"), query, strNew("20170606T121212Z"), header, HASH_TYPE_SHA256_ZERO_STR),
            "generate authorization");
        TEST_RESULT_STR_Z(
            httpHeaderGet(header, strNew("authorization")),
            "AWS4-HMAC-SHA256 Credential=AKIAIOSFODNN7EXAMPLE/20170606/us-east-1/s3/aws4_request,"
                "SignedHeaders=host;x-amz-content-sha256;x-amz-date;x-amz-security-token,"
                "Signature=85278841678ccbc0f137759265030d7b5e237868dd36eea658426b18344d1685",
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
                    hrnServerRunP(
                        ioFdReadNew(strNew("s3 server read"), HARNESS_FORK_CHILD_READ(), 5000), hrnServerProtocolTls, .port = port),
                    "s3 server run");
            }
            HARNESS_FORK_CHILD_END();

            HARNESS_FORK_CHILD_BEGIN(0, true)
            {
                TEST_RESULT_VOID(
                    hrnServerRunP(
                        ioFdReadNew(strNew("auth server read"), HARNESS_FORK_CHILD_READ(), 5000), hrnServerProtocolSocket,
                        .port = authPort),
                    "auth server run");
            }
            HARNESS_FORK_CHILD_END();

            HARNESS_FORK_PARENT_BEGIN()
            {
                IoWrite *service = hrnServerScriptBegin(
                    ioFdWriteNew(strNew("s3 client write"), HARNESS_FORK_PARENT_WRITE_PROCESS(0), 2000));
                IoWrite *auth = hrnServerScriptBegin(
                    ioFdWriteNew(strNew("auth client write"), HARNESS_FORK_PARENT_WRITE_PROCESS(1), 2000));

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("config with keys, token, and host with custom port");

                StringList *argList = strLstDup(commonArgList);
                strLstAdd(argList, strNewFmt("--" CFGOPT_REPO1_S3_HOST "=%s:%u", strZ(host), port));
                setenv("PGBACKREST_" CFGOPT_REPO1_S3_KEY, strZ(accessKey), true);
                setenv("PGBACKREST_" CFGOPT_REPO1_S3_KEY_SECRET, strZ(secretAccessKey), true);
                setenv("PGBACKREST_" CFGOPT_REPO1_S3_TOKEN, strZ(securityToken), true);
                harnessCfgLoad(cfgCmdArchivePush, argList);

                Storage *s3 = storageRepoGet(STORAGE_S3_TYPE_STR, true);
                StorageS3 *driver = (StorageS3 *)s3->driver;

                TEST_RESULT_STR(s3->path, path, "check path");
                TEST_RESULT_BOOL(storageFeature(s3, storageFeaturePath), false, "check path feature");
                TEST_RESULT_BOOL(storageFeature(s3, storageFeatureCompress), false, "check compress feature");

                // Coverage for noop functions
                // -----------------------------------------------------------------------------------------------------------------
                TEST_RESULT_VOID(storagePathSyncP(s3, strNew("path")), "path sync is a noop");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("ignore missing file");

                hrnServerScriptAccept(service);
                testRequestP(service, s3, HTTP_VERB_GET, "/fi%26le.txt");
                testResponseP(service, .code = 404);

                TEST_RESULT_PTR(storageGetP(storageNewReadP(s3, strNew("fi&le.txt"), .ignoreMissing = true)), NULL, "get file");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("error on missing file");

                testRequestP(service, s3, HTTP_VERB_GET, "/file.txt");
                testResponseP(service, .code = 404);

                TEST_ERROR(
                    storageGetP(storageNewReadP(s3, strNew("file.txt"))), FileMissingError,
                    "unable to open '/file.txt': No such file or directory");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("get file");

                testRequestP(service, s3, HTTP_VERB_GET, "/file.txt");
                testResponseP(service, .content = "this is a sample file");

                TEST_RESULT_STR_Z(
                    strNewBuf(storageGetP(storageNewReadP(s3, strNew("file.txt")))), "this is a sample file", "get file");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("get zero-length file");

                testRequestP(service, s3, HTTP_VERB_GET, "/file0.txt");
                testResponseP(service);

                TEST_RESULT_STR_Z(strNewBuf(storageGetP(storageNewReadP(s3, strNew("file0.txt")))), "", "get zero-length file");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("switch to temp credentials");

                hrnServerScriptClose(service);

                argList = strLstDup(commonArgList);
                strLstAdd(argList, strNewFmt("--" CFGOPT_REPO1_S3_HOST "=%s:%u", strZ(host), port));
                strLstAdd(argList, strNewFmt("--" CFGOPT_REPO1_S3_ROLE "=%s", strZ(credRole)));
                strLstAddZ(argList, "--" CFGOPT_REPO1_S3_KEY_TYPE "=" STORAGE_S3_KEY_TYPE_TEMP);
                harnessCfgLoad(cfgCmdArchivePush, argList);

                s3 = storageRepoGet(STORAGE_S3_TYPE_STR, true);
                driver = (StorageS3 *)s3->driver;

                TEST_RESULT_STR(s3->path, path, "check path");
                TEST_RESULT_STR(driver->credRole, credRole, "check role");
                TEST_RESULT_BOOL(storageFeature(s3, storageFeaturePath), false, "check path feature");
                TEST_RESULT_BOOL(storageFeature(s3, storageFeatureCompress), false, "check compress feature");

                // Set partSize to a small value for testing
                driver->partSize = 16;

                // Testing requires the auth http client to be redirected
                driver->credHost = hrnServerHost();
                driver->credHttpClient = httpClientNew(sckClientNew(host, authPort, 5000), 5000);

                // Now that we have checked the role when set explicitly, null it out to make sure it is retrieved automatically
                driver->credRole = NULL;

                hrnServerScriptAccept(service);

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("error when retrieving role");

                hrnServerScriptAccept(auth);

                testRequestP(auth, NULL, HTTP_VERB_GET, S3_CREDENTIAL_URI);
                testResponseP(auth, .http = "1.0", .code = 301);

                hrnServerScriptClose(auth);

                TEST_ERROR_FMT(
                    storageGetP(storageNewReadP(s3, strNew("file.txt"))), ProtocolError,
                    "HTTP request failed with 301:\n"
                        "*** URI/Query ***:\n"
                        "/latest/meta-data/iam/security-credentials\n"
                        "*** Request Headers ***:\n"
                        "content-length: 0\n"
                        "host: %s",
                    strZ(hrnServerHost()));

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("missing role");

                hrnServerScriptAccept(auth);

                testRequestP(auth, NULL, HTTP_VERB_GET, S3_CREDENTIAL_URI);
                testResponseP(auth, .http = "1.0", .code = 404);

                hrnServerScriptClose(auth);

                TEST_ERROR(
                    storageGetP(storageNewReadP(s3, strNew("file.txt"))), ProtocolError,
                    "role to retrieve temporary credentials not found\n"
                        "HINT: is a valid IAM role associated with this instance?");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("error when retrieving temp credentials");

                hrnServerScriptAccept(auth);

                testRequestP(auth, NULL, HTTP_VERB_GET, S3_CREDENTIAL_URI);
                testResponseP(auth, .http = "1.0", .content = strZ(credRole));

                hrnServerScriptClose(auth);
                hrnServerScriptAccept(auth);

                testRequestP(auth, NULL, HTTP_VERB_GET, strZ(strNewFmt(S3_CREDENTIAL_URI "/%s", strZ(credRole))));
                testResponseP(auth, .http = "1.0", .code = 300);

                hrnServerScriptClose(auth);

                TEST_ERROR_FMT(
                    storageGetP(storageNewReadP(s3, strNew("file.txt"))), ProtocolError,
                    "HTTP request failed with 300:\n"
                        "*** URI/Query ***:\n"
                        "/latest/meta-data/iam/security-credentials/credrole\n"
                        "*** Request Headers ***:\n"
                        "content-length: 0\n"
                        "host: %s",
                    strZ(hrnServerHost()));

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("invalid temp credentials role");

                hrnServerScriptAccept(auth);

                testRequestP(auth, NULL, HTTP_VERB_GET, strZ(strNewFmt(S3_CREDENTIAL_URI "/%s", strZ(credRole))));
                testResponseP(auth, .http = "1.0", .code = 404);

                hrnServerScriptClose(auth);

                TEST_ERROR_FMT(
                    storageGetP(storageNewReadP(s3, strNew("file.txt"))), ProtocolError,
                    "role '%s' not found\n"
                        "HINT: is '%s' a valid IAM role associated with this instance?",
                    strZ(credRole), strZ(credRole));

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("invalid code when retrieving temp credentials");

                hrnServerScriptAccept(auth);

                testRequestP(auth, NULL, HTTP_VERB_GET, strZ(strNewFmt(S3_CREDENTIAL_URI "/%s", strZ(credRole))));
                testResponseP(auth, .http = "1.0", .content = "{\"Code\":\"IAM role is not configured\"}");

                hrnServerScriptClose(auth);

                TEST_ERROR(
                    storageGetP(storageNewReadP(s3, strNew("file.txt"))), FormatError,
                    "unable to retrieve temporary credentials: IAM role is not configured");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("non-404 error");

                hrnServerScriptAccept(auth);

                testRequestP(auth, NULL, HTTP_VERB_GET, strZ(strNewFmt(S3_CREDENTIAL_URI "/%s", strZ(credRole))));
                testResponseP(
                    auth,
                    .content = strZ(
                        strNewFmt(
                            "{\"Code\":\"Success\",\"AccessKeyId\":\"x\",\"SecretAccessKey\":\"y\",\"Token\":\"z\""
                                ",\"Expiration\":\"%s\"}",
                            strZ(testS3DateTime(time(NULL) + (S3_CREDENTIAL_RENEW_SEC - 1))))));

                hrnServerScriptClose(auth);

                testRequestP(service, s3, HTTP_VERB_GET, "/file.txt", .accessKey = "x", .securityToken = "z");
                testResponseP(service, .code = 303, .content = "CONTENT");

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
                    "x-amz-security-token: <redacted>\n"
                    "*** Response Headers ***:\n"
                    "content-length: 7\n"
                    "*** Response Content ***:\n"
                    "CONTENT")

                // Check that temp credentials were set
                TEST_RESULT_STR_Z(driver->accessKey, "x", "check access key");
                TEST_RESULT_STR_Z(driver->secretAccessKey, "y", "check secret access key");
                TEST_RESULT_STR_Z(driver->securityToken, "z", "check security token");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("write file in one part");

                hrnServerScriptAccept(auth);

                testRequestP(auth, NULL, HTTP_VERB_GET, strZ(strNewFmt(S3_CREDENTIAL_URI "/%s", strZ(credRole))));
                testResponseP(
                    auth,
                    .content = strZ(
                        strNewFmt(
                            "{\"Code\":\"Success\",\"AccessKeyId\":\"xx\",\"SecretAccessKey\":\"yy\",\"Token\":\"zz\""
                                ",\"Expiration\":\"%s\"}",
                            strZ(testS3DateTime(time(NULL) + (S3_CREDENTIAL_RENEW_SEC * 2))))));

                hrnServerScriptClose(auth);

                testRequestP(service, s3, HTTP_VERB_PUT, "/file.txt", .content = "ABCD", .accessKey = "xx", .securityToken = "zz");
                testResponseP(service);

                // Make a copy of the signing key to verify that it gets changed when the keys are updated
                const Buffer *oldSigningKey = bufDup(driver->signingKey);

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

                // Check that temp credentials were changed
                TEST_RESULT_STR_Z(driver->accessKey, "xx", "check access key");
                TEST_RESULT_STR_Z(driver->secretAccessKey, "yy", "check secret access key");
                TEST_RESULT_STR_Z(driver->securityToken, "zz", "check security token");

                // Check that the signing key changed
                TEST_RESULT_BOOL(bufEq(driver->signingKey, oldSigningKey), false, "signing key changed");

                // Auth service no longer needed
                hrnServerScriptEnd(auth);

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("write zero-length file");

                testRequestP(service, s3, HTTP_VERB_PUT, "/file.txt", .content = "");
                testResponseP(service);

                TEST_ASSIGN(write, storageNewWriteP(s3, strNew("file.txt")), "new write");
                TEST_RESULT_VOID(storagePutP(write, NULL), "write");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("write file in chunks with nothing left over on close");

                testRequestP(service, s3, HTTP_VERB_POST, "/file.txt?uploads=");
                testResponseP(
                    service,
                    .content =
                        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
                        "<InitiateMultipartUploadResult xmlns=\"http://s3.amazonaws.com/doc/2006-03-01/\">"
                        "<Bucket>bucket</Bucket>"
                        "<Key>file.txt</Key>"
                        "<UploadId>WxRt</UploadId>"
                        "</InitiateMultipartUploadResult>");

                testRequestP(service, s3, HTTP_VERB_PUT, "/file.txt?partNumber=1&uploadId=WxRt", .content = "1234567890123456");
                testResponseP(service, .header = "etag:WxRt1");

                testRequestP(service, s3, HTTP_VERB_PUT, "/file.txt?partNumber=2&uploadId=WxRt", .content = "7890123456789012");
                testResponseP(service, .header = "eTag:WxRt2");

                testRequestP(
                    service, s3, HTTP_VERB_POST, "/file.txt?uploadId=WxRt",
                    .content =
                        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                        "<CompleteMultipartUpload>"
                        "<Part><PartNumber>1</PartNumber><ETag>WxRt1</ETag></Part>"
                        "<Part><PartNumber>2</PartNumber><ETag>WxRt2</ETag></Part>"
                        "</CompleteMultipartUpload>\n");
                testResponseP(service);

                TEST_ASSIGN(write, storageNewWriteP(s3, strNew("file.txt")), "new write");
                TEST_RESULT_VOID(storagePutP(write, BUFSTRDEF("12345678901234567890123456789012")), "write");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("write file in chunks with something left over on close");

                testRequestP(service, s3, HTTP_VERB_POST, "/file.txt?uploads=");
                testResponseP(
                    service,
                    .content =
                        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
                        "<InitiateMultipartUploadResult xmlns=\"http://s3.amazonaws.com/doc/2006-03-01/\">"
                        "<Bucket>bucket</Bucket>"
                        "<Key>file.txt</Key>"
                        "<UploadId>RR55</UploadId>"
                        "</InitiateMultipartUploadResult>");

                testRequestP(service, s3, HTTP_VERB_PUT, "/file.txt?partNumber=1&uploadId=RR55", .content = "1234567890123456");
                testResponseP(service, .header = "etag:RR551");

                testRequestP(service, s3, HTTP_VERB_PUT, "/file.txt?partNumber=2&uploadId=RR55", .content = "7890");
                testResponseP(service, .header = "eTag:RR552");

                testRequestP(
                    service, s3, HTTP_VERB_POST, "/file.txt?uploadId=RR55",
                    .content =
                        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                        "<CompleteMultipartUpload>"
                        "<Part><PartNumber>1</PartNumber><ETag>RR551</ETag></Part>"
                        "<Part><PartNumber>2</PartNumber><ETag>RR552</ETag></Part>"
                        "</CompleteMultipartUpload>\n");
                testResponseP(service);

                TEST_ASSIGN(write, storageNewWriteP(s3, strNew("file.txt")), "new write");
                TEST_RESULT_VOID(storagePutP(write, BUFSTRDEF("12345678901234567890")), "write");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("file missing");

                testRequestP(service, s3, HTTP_VERB_HEAD, "/BOGUS");
                testResponseP(service, .code = 404);

                TEST_RESULT_BOOL(storageExistsP(s3, strNew("BOGUS")), false, "check");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("info for missing file");

                // File missing
                testRequestP(service, s3, HTTP_VERB_HEAD, "/BOGUS");
                testResponseP(service, .code = 404);

                TEST_RESULT_BOOL(storageInfoP(s3, strNew("BOGUS"), .ignoreMissing = true).exists, false, "file does not exist");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("info for file");

                testRequestP(service, s3, HTTP_VERB_HEAD, "/subdir/file1.txt");
                testResponseP(service, .header = "content-length:9999\r\nLast-Modified: Wed, 21 Oct 2015 07:28:00 GMT");

                StorageInfo info;
                TEST_ASSIGN(info, storageInfoP(s3, strNew("subdir/file1.txt")), "file exists");
                TEST_RESULT_BOOL(info.exists, true, "    check exists");
                TEST_RESULT_UINT(info.type, storageTypeFile, "    check type");
                TEST_RESULT_UINT(info.size, 9999, "    check exists");
                TEST_RESULT_INT(info.timeModified, 1445412480, "    check time");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("info check existence only");

                testRequestP(service, s3, HTTP_VERB_HEAD, "/subdir/file2.txt");
                testResponseP(service, .header = "content-length:777\r\nLast-Modified: Wed, 22 Oct 2015 07:28:00 GMT");

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

                testRequestP(service, s3, HTTP_VERB_GET, "/?delimiter=%2F&list-type=2");
                testResponseP(service, .code = 344);

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
                    "x-amz-security-token: <redacted>");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("error with xml");

                testRequestP(service, s3, HTTP_VERB_GET, "/?delimiter=%2F&list-type=2");
                testResponseP(
                    service, .code = 344,
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
                    "x-amz-security-token: <redacted>\n"
                    "*** Response Headers ***:\n"
                    "content-length: 79\n"
                    "*** Response Content ***:\n"
                    "<?xml version=\"1.0\" encoding=\"UTF-8\"?><Error><Code>SomeOtherCode</Code></Error>");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("list basic level");

                testRequestP(service, s3, HTTP_VERB_GET, "/?delimiter=%2F&list-type=2&prefix=path%2Fto%2F");
                testResponseP(
                    service,
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

                testRequestP(service, s3, HTTP_VERB_GET, "/?delimiter=%2F&list-type=2");
                testResponseP(
                    service,
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

                testRequestP(service, s3, HTTP_VERB_GET, "/?delimiter=%2F&list-type=2&prefix=test");
                testResponseP(
                    service,
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

                testRequestP(service, s3, HTTP_VERB_GET, "/?delimiter=%2F&list-type=2&prefix=path%2Fto%2F");
                testResponseP(
                    service,
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
                    service, s3, HTTP_VERB_GET,
                    "/?continuation-token=1ueGcxLPRx1Tr%2FXYExHnhbYLgveDs2J%2Fwm36Hy4vbOwM%3D&delimiter=%2F&list-type=2"
                        "&prefix=path%2Fto%2F");
                testResponseP(
                    service,
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

                testRequestP(service, s3, HTTP_VERB_GET, "/?delimiter=%2F&list-type=2&prefix=path%2Fto%2Ftest");
                testResponseP(
                    service,
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

                hrnServerScriptClose(service);

                argList = strLstDup(commonArgList);
                strLstAddZ(argList, "--" CFGOPT_REPO1_S3_URI_STYLE "=" STORAGE_S3_URI_STYLE_PATH);
                strLstAdd(argList, strNewFmt("--" CFGOPT_REPO1_S3_HOST "=%s", strZ(host)));
                strLstAdd(argList, strNewFmt("--" CFGOPT_REPO1_S3_PORT "=%u", port));
                setenv("PGBACKREST_" CFGOPT_REPO1_S3_KEY, strZ(accessKey), true);
                setenv("PGBACKREST_" CFGOPT_REPO1_S3_KEY_SECRET, strZ(secretAccessKey), true);
                unsetenv("PGBACKREST_" CFGOPT_REPO1_S3_TOKEN);
                harnessCfgLoad(cfgCmdArchivePush, argList);

                s3 = storageRepoGet(STORAGE_S3_TYPE_STR, true);
                driver = (StorageS3 *)s3->driver;

                // Set deleteMax to a small value for testing
                driver->deleteMax = 2;

                hrnServerScriptAccept(service);

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("error when no recurse because there are no paths");

                TEST_ERROR(
                    storagePathRemoveP(s3, strNew("/")), AssertError,
                    "assertion 'param.recurse || storageFeature(this, storageFeaturePath)' failed");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("remove files from root");

                testRequestP(service, s3, HTTP_VERB_GET, "/bucket/?list-type=2");
                testResponseP(
                    service,
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
                    service, s3, HTTP_VERB_POST, "/bucket/?delete=",
                    .content =
                        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                        "<Delete><Quiet>true</Quiet>"
                        "<Object><Key>test1.txt</Key></Object>"
                        "<Object><Key>path1/xxx.zzz</Key></Object>"
                        "</Delete>\n");
                testResponseP(
                    service, .content = "<DeleteResult xmlns=\"http://s3.amazonaws.com/doc/2006-03-01/\"></DeleteResult>");

                TEST_RESULT_VOID(storagePathRemoveP(s3, strNew("/"), .recurse = true), "remove");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("remove files in empty subpath (nothing to do)");

                testRequestP(service, s3, HTTP_VERB_GET, "/bucket/?list-type=2&prefix=path%2F");
                testResponseP(
                    service,
                    .content =
                        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
                        "<ListBucketResult xmlns=\"http://s3.amazonaws.com/doc/2006-03-01/\">"
                        "</ListBucketResult>");

                TEST_RESULT_VOID(storagePathRemoveP(s3, strNew("/path"), .recurse = true), "remove");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("remove files with continuation");

                testRequestP(service, s3, HTTP_VERB_GET, "/bucket/?list-type=2&prefix=path%2Fto%2F");
                testResponseP(
                    service,
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

                testRequestP(service, s3, HTTP_VERB_GET, "/bucket/?continuation-token=continue&list-type=2&prefix=path%2Fto%2F");
                testResponseP(
                    service,
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
                    service, s3, HTTP_VERB_POST, "/bucket/?delete=",
                    .content =
                        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                        "<Delete><Quiet>true</Quiet>"
                        "<Object><Key>path/to/test1.txt</Key></Object>"
                        "<Object><Key>path/to/test3.txt</Key></Object>"
                        "</Delete>\n");
                testResponseP(service);

                testRequestP(
                    service, s3, HTTP_VERB_POST, "/bucket/?delete=",
                    .content =
                        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                        "<Delete><Quiet>true</Quiet>"
                        "<Object><Key>path/to/test2.txt</Key></Object>"
                        "</Delete>\n");
                testResponseP(service);

                TEST_RESULT_VOID(storagePathRemoveP(s3, strNew("/path/to"), .recurse = true), "remove");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("remove error");

                testRequestP(service, s3, HTTP_VERB_GET, "/bucket/?list-type=2&prefix=path%2F");
                testResponseP(
                    service,
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
                    service, s3, HTTP_VERB_POST, "/bucket/?delete=",
                    .content =
                        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                        "<Delete><Quiet>true</Quiet>"
                        "<Object><Key>path/sample.txt</Key></Object>"
                        "<Object><Key>path/sample2.txt</Key></Object>"
                        "</Delete>\n");
                testResponseP(
                    service,
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

                testRequestP(service, s3, HTTP_VERB_DELETE, "/bucket/path/to/test.txt");
                testResponseP(service, .code = 204);

                TEST_RESULT_VOID(storageRemoveP(s3, strNew("/path/to/test.txt")), "remove");

                // -----------------------------------------------------------------------------------------------------------------
                hrnServerScriptEnd(service);
            }
            HARNESS_FORK_PARENT_END();
        }
        HARNESS_FORK_END();
    }

    FUNCTION_HARNESS_RESULT_VOID();
}
