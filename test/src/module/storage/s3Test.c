/***********************************************************************************************************************************
Test S3 Storage
***********************************************************************************************************************************/
#include <unistd.h>

#include "common/io/fdRead.h"
#include "common/io/fdWrite.h"
#include "storage/helper.h"
#include "version.h"

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
    const char *range;
    const char *kms;
    const char *ttl;
    const char *token;
} TestRequestParam;

#define testRequestP(write, s3, verb, path, ...)                                                                                   \
    testRequest(write, s3, verb, path, (TestRequestParam){VAR_PARAM_INIT, __VA_ARGS__})

static void
testRequest(IoWrite *write, Storage *s3, const char *verb, const char *path, TestRequestParam param)
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
    String *request = strCatFmt(strNew(), "%s %s HTTP/1.1\r\nuser-agent:" PROJECT_NAME "/" PROJECT_VERSION "\r\n", verb, path);

    // Add authorization header when s3 service
    if (s3 != NULL)
    {
        strCatFmt(
            request,
            "authorization:AWS4-HMAC-SHA256 Credential=%s/\?\?\?\?\?\?\?\?/us-east-1/s3/aws4_request,SignedHeaders=",
            param.accessKey == NULL ? strZ(driver->accessKey) : param.accessKey);

        if (param.content != NULL)
            strCatZ(request, "content-md5;");

        strCatZ(request, "host;");

        if (param.range != NULL)
            strCatZ(request, "range;");

        strCatZ(request, "x-amz-content-sha256;x-amz-date");

        if (securityToken != NULL)
            strCatZ(request, ";x-amz-security-token");

        if (param.kms != NULL)
            strCatZ(request, ";x-amz-server-side-encryption;x-amz-server-side-encryption-aws-kms-key-id");

        strCatZ(request, ",Signature=????????????????????????????????????????????????????????????????\r\n");
    }

    // Add content-length
    strCatFmt(request, "content-length:%zu\r\n", param.content != NULL ? strlen(param.content) : 0);

    // Add md5
    if (param.content != NULL)
    {
        strCatFmt(
            request, "content-md5:%s\r\n", strZ(strNewEncode(encodeBase64, cryptoHashOne(hashTypeMd5, BUFSTRZ(param.content)))));
    }

    // Add host
    if (s3 != NULL)
    {
        if (driver->uriStyle == storageS3UriStyleHost)
            strCatZ(request, "host:bucket." S3_TEST_HOST "\r\n");
        else
            strCatZ(request, "host:" S3_TEST_HOST "\r\n");
    }
    else
        strCatFmt(request, "host:%s\r\n", strZ(hrnServerHost()));

    // Add range
    if (param.range != NULL)
        strCatFmt(request, "range:bytes=%s\r\n", param.range);

    // Add content checksum and date if s3 service
    if (s3 != NULL)
    {
        // Add content sha256 and date
        strCatFmt(
            request,
            "x-amz-content-sha256:%s\r\n"
                "x-amz-date:????????T??????Z" "\r\n",
            param.content == NULL ? HASH_TYPE_SHA256_ZERO : strZ(bufHex(cryptoHashOne(hashTypeSha256, BUFSTRZ(param.content)))));

        // Add security token
        if (securityToken != NULL)
            strCatFmt(request, "x-amz-security-token:%s\r\n", securityToken);
    }

    // Add kms key
    if (param.kms != NULL)
    {
        strCatZ(request, "x-amz-server-side-encryption:aws:kms\r\n");
        strCatFmt(request, "x-amz-server-side-encryption-aws-kms-key-id:%s\r\n", param.kms);
    }

    // Add metadata token
    if (param.token != NULL)
        strCatFmt(request, "x-aws-ec2-metadata-token:%s\r\n", param.token);

    // Add metadata token ttl
    if (param.ttl != NULL)
        strCatFmt(request, "x-aws-ec2-metadata-token-ttl-seconds:%s\r\n", param.ttl);

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
    String *response = strCatFmt(strNew(), "HTTP/%s %u ", param.http == NULL ? "1.1" : param.http, param.code);

    // Add reason for some codes
    switch (param.code)
    {
        case 200:
            strCatZ(response, "OK");
            break;
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

    FUNCTION_HARNESS_RETURN(STRING, strNewZ(buffer));
}

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
static void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    // Set storage helper
    static const StorageHelper storageHelperList[] = {STORAGE_S3_HELPER, STORAGE_END_HELPER};
    storageHelperInit(storageHelperList);

    // Test strings
    const String *path = STRDEF("/");
    const String *bucket = STRDEF("bucket");
    const String *region = STRDEF("us-east-1");
    const String *endPoint = STRDEF("s3.amazonaws.com");
    const String *host = hrnServerHost();
    const unsigned int port = hrnServerPort(0);
    const unsigned int authPort = hrnServerPort(1);
    const String *accessKey = STRDEF("AKIAIOSFODNN7EXAMPLE");
    const String *secretAccessKey = STRDEF("wJalrXUtnFEMI/K7MDENG/bPxRfiCYEXAMPLEKEY");
    const String *securityToken = STRDEF(
        "AQoDYXdzEPT//////////wEXAMPLEtc764bNrC9SAPBSM22wDOk4x4HIZ8j4FZTwdQWLWsKWHGBuFqwAeMicRXmxfpSPfIeoIYRqTflfKD8YUuwthAx7mSEI/q"
        "kPpKPi/kMcGdQrmGdeehM4IC1NtBmUpp2wUE8phUZampKsburEDy0KPkyQDYwT7WZ0wq5VSXDvp75YU9HFvlRd8Tx6q6fE8YQcHNVXAkiY9q6d+xo0rKwT38xV"
        "qr7ZD0u0iPPkUL64lIZbqBAz+scqKmlzm8FDrypNC9Yjc8fPOLn9FX9KSYvKTr4rvx3iSIlTJabIQwj2ICCR/oLxBA==");
    const String *credRole = STRDEF("credrole");

    // Config settings that are required for every test (without endpoint for special tests)
    StringList *commonArgWithoutEndpointList = strLstNew();
    hrnCfgArgRawZ(commonArgWithoutEndpointList, cfgOptStanza, "db");
    hrnCfgArgRawZ(commonArgWithoutEndpointList, cfgOptRepoType, "s3");
    hrnCfgArgRaw(commonArgWithoutEndpointList, cfgOptRepoPath, path);
    hrnCfgArgRaw(commonArgWithoutEndpointList, cfgOptRepoS3Bucket, bucket);
    hrnCfgArgRaw(commonArgWithoutEndpointList, cfgOptRepoS3Region, region);

    // TLS can only be verified in a container
    if (!TEST_IN_CONTAINER)
        hrnCfgArgRawBool(commonArgWithoutEndpointList, cfgOptRepoStorageVerifyTls, false);

    // Config settings that are required for every test (with endpoint)
    StringList *commonArgList = strLstDup(commonArgWithoutEndpointList);
    hrnCfgArgRaw(commonArgList, cfgOptRepoS3Endpoint, endPoint);

    // Secure options must be loaded into environment variables
    hrnCfgEnvRaw(cfgOptRepoS3Key, accessKey);
    hrnCfgEnvRaw(cfgOptRepoS3KeySecret, secretAccessKey);

    // *****************************************************************************************************************************
    if (testBegin("storageS3DateTime() and storageS3Auth()"))
    {
        TEST_RESULT_STR_Z(storageS3DateTime(1491267845), "20170404T010405Z", "static date");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("config without token");

        StringList *argList = strLstDup(commonArgList);
        HRN_CFG_LOAD(cfgCmdArchivePush, argList);

        StorageS3 *driver = (StorageS3 *)storageDriver(storageRepoGet(0, false));

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
                    ", timeoutConnect: 60000, timeoutSession: 60000}}, timeoutConnect: 60000, timeoutSession: 60000"
                    ", verifyPeer: %s}}, reusable: 0, timeout: 60000}",
                cvtBoolToConstZ(TEST_IN_CONTAINER)),
            "check http client");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("auth with token");

        HttpHeader *header = httpHeaderNew(NULL);
        HttpQuery *query = httpQueryNewP();
        httpQueryAdd(query, STRDEF("list-type"), STRDEF("2"));

        TEST_RESULT_VOID(
            storageS3Auth(driver, STRDEF("GET"), STRDEF("/"), query, STRDEF("20170606T121212Z"), header, HASH_TYPE_SHA256_ZERO_STR),
            "generate authorization");
        TEST_RESULT_STR_Z(
            httpHeaderGet(header, STRDEF("authorization")),
            "AWS4-HMAC-SHA256 Credential=AKIAIOSFODNN7EXAMPLE/20170606/us-east-1/s3/aws4_request,"
                "SignedHeaders=host;x-amz-content-sha256;x-amz-date,"
                "Signature=cb03bf1d575c1f8904dabf0e573990375340ab293ef7ad18d049fc1338fd89b3",
            "check authorization header");

        // Test again to be sure cache signing key is used
        const Buffer *lastSigningKey = driver->signingKey;

        TEST_RESULT_VOID(
            storageS3Auth(driver, STRDEF("GET"), STRDEF("/"), query, STRDEF("20170606T121212Z"), header, HASH_TYPE_SHA256_ZERO_STR),
            "generate authorization");
        TEST_RESULT_STR_Z(
            httpHeaderGet(header, STRDEF("authorization")),
            "AWS4-HMAC-SHA256 Credential=AKIAIOSFODNN7EXAMPLE/20170606/us-east-1/s3/aws4_request,"
                "SignedHeaders=host;x-amz-content-sha256;x-amz-date,"
                "Signature=cb03bf1d575c1f8904dabf0e573990375340ab293ef7ad18d049fc1338fd89b3",
            "check authorization header");
        TEST_RESULT_BOOL(driver->signingKey == lastSigningKey, true, "check signing key was reused");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("change date to generate new signing key");

        TEST_RESULT_VOID(
            storageS3Auth(driver, STRDEF("GET"), STRDEF("/"), query, STRDEF("20180814T080808Z"), header, HASH_TYPE_SHA256_ZERO_STR),
            "generate authorization");
        TEST_RESULT_STR_Z(
            httpHeaderGet(header, STRDEF("authorization")),
            "AWS4-HMAC-SHA256 Credential=AKIAIOSFODNN7EXAMPLE/20180814/us-east-1/s3/aws4_request,"
                "SignedHeaders=host;x-amz-content-sha256;x-amz-date,"
                "Signature=d0fa9c36426eb94cdbaf287a7872c7a3b6c913f523163d0d7debba0758e36f49",
            "check authorization header");
        TEST_RESULT_BOOL(driver->signingKey != lastSigningKey, true, "check signing key was regenerated");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("config with token, endpoint with custom port, and ca-file/path");

        argList = strLstDup(commonArgWithoutEndpointList);
        hrnCfgArgRawZ(argList, cfgOptRepoS3Endpoint, "custom.endpoint:333");
        hrnCfgArgRawZ(argList, cfgOptRepoStorageCaPath, "/path/to/cert");
        hrnCfgArgRawZ(argList, cfgOptRepoStorageCaFile, HRN_SERVER_CA);
        hrnCfgEnvRaw(cfgOptRepoS3Token, securityToken);
        HRN_CFG_LOAD(cfgCmdArchivePush, argList);

        driver = (StorageS3 *)storageDriver(storageRepoGet(0, false));

        TEST_RESULT_STR(driver->securityToken, securityToken, "check security token");
        TEST_RESULT_STR(
            httpClientToLog(driver->httpClient),
            strNewFmt(
                "{ioClient: {type: tls, driver: {ioClient: {type: socket, driver: {host: bucket.custom.endpoint, port: 333"
                    ", timeoutConnect: 60000, timeoutSession: 60000}}, timeoutConnect: 60000, timeoutSession: 60000"
                    ", verifyPeer: %s}}, reusable: 0, timeout: 60000}",
                cvtBoolToConstZ(TEST_IN_CONTAINER)),
            "check http client");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("auth with token");

        TEST_RESULT_VOID(
            storageS3Auth(driver, STRDEF("GET"), STRDEF("/"), query, STRDEF("20170606T121212Z"), header, HASH_TYPE_SHA256_ZERO_STR),
            "generate authorization");
        TEST_RESULT_STR_Z(
            httpHeaderGet(header, STRDEF("authorization")),
            "AWS4-HMAC-SHA256 Credential=AKIAIOSFODNN7EXAMPLE/20170606/us-east-1/s3/aws4_request,"
                "SignedHeaders=host;x-amz-content-sha256;x-amz-date;x-amz-security-token,"
                "Signature=85278841678ccbc0f137759265030d7b5e237868dd36eea658426b18344d1685",
            "check authorization header");
    }

    // *****************************************************************************************************************************
    if (testBegin("storageS3*(), StorageReadS3, and StorageWriteS3"))
    {
        HRN_FORK_BEGIN()
        {
            HRN_FORK_CHILD_BEGIN(.prefix = "s3 server", .timeout = 5000)
            {
                TEST_RESULT_VOID(hrnServerRunP(HRN_FORK_CHILD_READ(), hrnServerProtocolTls, .port = port), "s3 server run");
            }
            HRN_FORK_CHILD_END();

            HRN_FORK_CHILD_BEGIN(.prefix = "auth server", .timeout = 10000)
            {
                TEST_RESULT_VOID(
                    hrnServerRunP(HRN_FORK_CHILD_READ(), hrnServerProtocolSocket, .port = authPort), "auth server run");
            }
            HRN_FORK_CHILD_END();

            HRN_FORK_PARENT_BEGIN()
            {
                // Do not use HRN_FORK_PARENT_WRITE() here so individual names can be assigned to help with debugging
                IoWrite *service = hrnServerScriptBegin(
                    ioFdWriteNewOpen(STRDEF("s3 client write"), HRN_FORK_PARENT_WRITE_FD(0), 2000));
                IoWrite *auth = hrnServerScriptBegin(
                    ioFdWriteNewOpen(STRDEF("auth client write"), HRN_FORK_PARENT_WRITE_FD(1), 2000));

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("config with keys, token, and host with custom port");

                StringList *argList = strLstDup(commonArgList);
                hrnCfgArgRawFmt(argList, cfgOptRepoStorageHost, "%s:%u", strZ(host), port);
                hrnCfgEnvRaw(cfgOptRepoS3Token, securityToken);
                HRN_CFG_LOAD(cfgCmdArchivePush, argList);

                Storage *s3 = storageRepoGet(0, true);
                StorageS3 *driver = (StorageS3 *)storageDriver(s3);

                TEST_RESULT_STR(s3->path, path, "check path");
                TEST_RESULT_BOOL(storageFeature(s3, storageFeaturePath), false, "check path feature");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("coverage for noop functions");

                TEST_RESULT_VOID(storagePathSyncP(s3, STRDEF("path")), "path sync is a noop");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("ignore missing file");

                hrnServerScriptAccept(service);
                testRequestP(service, s3, HTTP_VERB_GET, "/fi%26le.txt");
                testResponseP(service, .code = 404);

                TEST_RESULT_PTR(storageGetP(storageNewReadP(s3, STRDEF("fi&le.txt"), .ignoreMissing = true)), NULL, "get file");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("error on missing file");

                testRequestP(service, s3, HTTP_VERB_GET, "/file.txt");
                testResponseP(service, .code = 404);

                TEST_ERROR(
                    storageGetP(storageNewReadP(s3, STRDEF("file.txt"))), FileMissingError,
                    "unable to open missing file '/file.txt' for read");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("get file with offset and limit");

                testRequestP(service, s3, HTTP_VERB_GET, "/file.txt", .range = "1-21");
                testResponseP(service, .content = "this is a sample file");

                TEST_RESULT_STR_Z(
                    strNewBuf(storageGetP(storageNewReadP(s3, STRDEF("file.txt"), .offset = 1, .limit = VARUINT64(21)))),
                    "this is a sample file", "get file");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("get zero-length file");

                testRequestP(service, s3, HTTP_VERB_GET, "/file0.txt");
                testResponseP(service);

                TEST_RESULT_STR_Z(strNewBuf(storageGetP(storageNewReadP(s3, STRDEF("file0.txt")))), "", "get zero-length file");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("switch to temp credentials");

                hrnServerScriptClose(service);

                argList = strLstDup(commonArgList);
                hrnCfgArgRawFmt(argList, cfgOptRepoStorageHost, "%s:%u", strZ(host), port);
                hrnCfgArgRaw(argList, cfgOptRepoS3Role, credRole);
                hrnCfgArgRawStrId(argList, cfgOptRepoS3KeyType, storageS3KeyTypeAuto);
                hrnCfgArgRawZ(argList, cfgOptRepoS3KmsKeyId, "kmskey1");
                HRN_CFG_LOAD(cfgCmdArchivePush, argList);

                s3 = storageRepoGet(0, true);
                driver = (StorageS3 *)storageDriver(s3);

                TEST_RESULT_STR(s3->path, path, "check path");
                TEST_RESULT_STR(driver->credRole, credRole, "check role");
                TEST_RESULT_BOOL(storageFeature(s3, storageFeaturePath), false, "check path feature");

                // Set partSize to a small value for testing
                driver->partSize = 16;

                // Testing requires the auth http client to be redirected
                driver->credHost = hrnServerHost();
                driver->credHttpClient = httpClientNew(sckClientNew(host, authPort, 5000, 5000), 5000);

                // Now that we have checked the role when set explicitly, null it out to make sure it is retrieved automatically
                driver->credRole = NULL;

                hrnServerScriptAccept(service);

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("error when retrieving role");

                hrnServerScriptAccept(auth);

                testRequestP(auth, NULL, HTTP_VERB_PUT, "/latest/api/token", .ttl = "15");
                testResponseP(auth, .http = "1.0", .content = "WtokenW");

                hrnServerScriptClose(auth);
                hrnServerScriptAccept(auth);

                testRequestP(auth, NULL, HTTP_VERB_GET, S3_CREDENTIAL_PATH, .token = "WtokenW");
                testResponseP(auth, .http = "1.0", .code = 301);

                hrnServerScriptClose(auth);

                TEST_ERROR_FMT(
                    storageGetP(storageNewReadP(s3, STRDEF("file.txt"))), ProtocolError,
                    "HTTP request failed with 301:\n"
                        "*** Path/Query ***:\n"
                        "GET /latest/meta-data/iam/security-credentials\n"
                        "*** Request Headers ***:\n"
                        "content-length: 0\n"
                        "host: %s\n"
                        "x-aws-ec2-metadata-token: WtokenW",
                    strZ(hrnServerHost()));

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("missing role and token");

                hrnServerScriptAccept(auth);

                testRequestP(auth, NULL, HTTP_VERB_PUT, "/latest/api/token", .ttl = "15");
                testResponseP(auth, .http = "1.0", .code = 301);

                hrnServerScriptClose(auth);
                hrnServerScriptAccept(auth);

                testRequestP(auth, NULL, HTTP_VERB_GET, S3_CREDENTIAL_PATH);
                testResponseP(auth, .http = "1.0", .code = 404);

                hrnServerScriptClose(auth);

                TEST_ERROR(
                    storageGetP(storageNewReadP(s3, STRDEF("file.txt"))), ProtocolError,
                    "role to retrieve temporary credentials not found\n"
                        "HINT: is a valid IAM role associated with this instance?");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("error when retrieving temp credentials and missing token");

                hrnServerScriptAccept(auth);

                testRequestP(auth, NULL, HTTP_VERB_PUT, "/latest/api/token", .ttl = "15");
                testResponseP(auth, .http = "1.0", .code = 301);

                hrnServerScriptClose(auth);
                hrnServerScriptAccept(auth);

                testRequestP(auth, NULL, HTTP_VERB_GET, S3_CREDENTIAL_PATH);
                testResponseP(auth, .http = "1.0", .content = strZ(credRole));

                hrnServerScriptClose(auth);
                hrnServerScriptAccept(auth);

                testRequestP(auth, NULL, HTTP_VERB_GET, zNewFmt(S3_CREDENTIAL_PATH "/%s", strZ(credRole)));
                testResponseP(auth, .http = "1.0", .code = 300);

                hrnServerScriptClose(auth);

                TEST_ERROR_FMT(
                    storageGetP(storageNewReadP(s3, STRDEF("file.txt"))), ProtocolError,
                    "HTTP request failed with 300:\n"
                        "*** Path/Query ***:\n"
                        "GET /latest/meta-data/iam/security-credentials/credrole\n"
                        "*** Request Headers ***:\n"
                        "content-length: 0\n"
                        "host: %s",
                    strZ(hrnServerHost()));

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("invalid temp credentials role and missing token");

                hrnServerScriptAccept(auth);

                testRequestP(auth, NULL, HTTP_VERB_PUT, "/latest/api/token", .ttl = "15");
                testResponseP(auth, .http = "1.0", .code = 301);

                hrnServerScriptClose(auth);
                hrnServerScriptAccept(auth);

                testRequestP(auth, NULL, HTTP_VERB_GET, zNewFmt(S3_CREDENTIAL_PATH "/%s", strZ(credRole)));
                testResponseP(auth, .http = "1.0", .code = 404);

                hrnServerScriptClose(auth);

                TEST_ERROR_FMT(
                    storageGetP(storageNewReadP(s3, STRDEF("file.txt"))), ProtocolError,
                    "role '%s' not found\n"
                        "HINT: is '%s' a valid IAM role associated with this instance?",
                    strZ(credRole), strZ(credRole));

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("invalid code when retrieving temp credentials");

                hrnServerScriptAccept(auth);

                testRequestP(auth, NULL, HTTP_VERB_PUT, "/latest/api/token", .ttl = "15");
                testResponseP(auth, .http = "1.0", .content = "XtokenX");

                hrnServerScriptClose(auth);
                hrnServerScriptAccept(auth);

                testRequestP(
                    auth, NULL, HTTP_VERB_GET, zNewFmt(S3_CREDENTIAL_PATH "/%s", strZ(credRole)), .token = "XtokenX");
                testResponseP(auth, .http = "1.0", .content = "{\"Code\":\"IAM role is not configured\"}");

                hrnServerScriptClose(auth);

                TEST_ERROR(
                    storageGetP(storageNewReadP(s3, STRDEF("file.txt"))), FormatError,
                    "unable to retrieve temporary credentials: IAM role is not configured");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("non-404 error");

                hrnServerScriptAccept(auth);

                testRequestP(auth, NULL, HTTP_VERB_PUT, "/latest/api/token", .ttl = "15");
                testResponseP(auth, .http = "1.0", .content = "YtokenY");

                hrnServerScriptClose(auth);
                hrnServerScriptAccept(auth);

                testRequestP(
                    auth, NULL, HTTP_VERB_GET, zNewFmt(S3_CREDENTIAL_PATH "/%s", strZ(credRole)), .token = "YtokenY");
                testResponseP(
                    auth,
                    .content = zNewFmt(
                        "{\"Code\":\"Success\",\"AccessKeyId\":\"x\",\"SecretAccessKey\":\"y\",\"Token\":\"z\""
                            ",\"Expiration\":\"%s\"}",
                        strZ(testS3DateTime(time(NULL) + (S3_CREDENTIAL_RENEW_SEC - 1)))));

                hrnServerScriptClose(auth);

                testRequestP(service, s3, HTTP_VERB_GET, "/file.txt", .accessKey = "x", .securityToken = "z");
                testResponseP(service, .code = 303, .content = "CONTENT");

                StorageRead *read = NULL;
                TEST_ASSIGN(read, storageNewReadP(s3, STRDEF("file.txt"), .ignoreMissing = true), "new read file");
                TEST_RESULT_BOOL(storageReadIgnoreMissing(read), true, "check ignore missing");
                TEST_RESULT_STR_Z(storageReadName(read), "/file.txt", "check name");

                TEST_ERROR(
                    ioReadOpen(storageReadIo(read)), ProtocolError,
                    "HTTP request failed with 303:\n"
                    "*** Path/Query ***:\n"
                    "GET /file.txt\n"
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

                testRequestP(auth, NULL, HTTP_VERB_PUT, "/latest/api/token", .ttl = "15");
                testResponseP(auth, .http = "1.0", .content = "ZtokenZ");

                hrnServerScriptClose(auth);
                hrnServerScriptAccept(auth);

                testRequestP(auth, NULL, HTTP_VERB_GET, zNewFmt(S3_CREDENTIAL_PATH "/%s", strZ(credRole)), .token = "ZtokenZ");
                testResponseP(
                    auth,
                    .content = zNewFmt(
                        "{\"Code\":\"Success\",\"AccessKeyId\":\"xx\",\"SecretAccessKey\":\"yy\",\"Token\":\"zz\""
                            ",\"Expiration\":\"%s\"}",
                        strZ(testS3DateTime(time(NULL) + (S3_CREDENTIAL_RENEW_SEC * 2)))));

                hrnServerScriptClose(auth);

                testRequestP(
                    service, s3, HTTP_VERB_PUT, "/file.txt", .content = "ABCD", .accessKey = "xx", .securityToken = "zz",
                    .kms = "kmskey1");
                testResponseP(service);

                // Make a copy of the signing key to verify that it gets changed when the keys are updated
                const Buffer *oldSigningKey = bufDup(driver->signingKey);

                StorageWrite *write = NULL;
                TEST_ASSIGN(write, storageNewWriteP(s3, STRDEF("file.txt")), "new write");
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

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("write zero-length file");

                testRequestP(service, s3, HTTP_VERB_PUT, "/file.txt", .content = "", .kms = "kmskey1");
                testResponseP(service);

                TEST_ASSIGN(write, storageNewWriteP(s3, STRDEF("file.txt")), "new write");
                TEST_RESULT_VOID(storagePutP(write, NULL), "write");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("write file in chunks with nothing left over on close");

                testRequestP(service, s3, HTTP_VERB_POST, "/file.txt?uploads=", .kms = "kmskey1");
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
                testResponseP(
                    service,
                    .content =
                        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
                        "<CompleteMultipartUploadResult><ETag>XXX</ETag></CompleteMultipartUploadResult>");

                TEST_ASSIGN(write, storageNewWriteP(s3, STRDEF("file.txt")), "new write");
                TEST_RESULT_VOID(storagePutP(write, BUFSTRDEF("12345678901234567890123456789012")), "write");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("error in success response of multipart upload");

                testRequestP(service, s3, HTTP_VERB_POST, "/file.txt?uploads=", .kms = "kmskey1");
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
                testResponseP(
                    service,
                    .content =
                        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
                        "<Error><Code>AccessDenied</Code><Message>Access Denied</Message></Error>");

                TEST_ASSIGN(write, storageNewWriteP(s3, STRDEF("file.txt")), "new write");
                TEST_ERROR(
                    storagePutP(write, BUFSTRDEF("12345678901234567890123456789012")), ProtocolError,
                    "HTTP request failed with 200 (OK):\n"
                    "*** Path/Query ***:\n"
                    "POST /file.txt?uploadId=WxRt\n"
                    "*** Request Headers ***:\n"
                    "authorization: <redacted>\n"
                    "content-length: 205\n"
                    "content-md5: 37smUM6Ah2/EjZbp420dPw==\n"
                    "host: bucket.s3.amazonaws.com\n"
                    "x-amz-content-sha256: 0838a79dfbddc2128d28fb4fa8d605e0a8e6d1355094000f39b6eb3feff4641f\n"
                    "x-amz-date: <redacted>\n"
                    "x-amz-security-token: <redacted>\n"
                    "*** Response Headers ***:\n"
                    "content-length: 110\n"
                        "*** Response Content ***:\n"
                    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
                        "<Error><Code>AccessDenied</Code><Message>Access Denied</Message></Error>");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("write file in chunks with something left over on close");

                testRequestP(service, s3, HTTP_VERB_POST, "/file.txt?uploads=", .kms = "kmskey1");
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
                testResponseP(
                    service,
                    .content =
                        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
                        "<CompleteMultipartUploadResult><ETag>XXX</ETag></CompleteMultipartUploadResult>");

                TEST_ASSIGN(write, storageNewWriteP(s3, STRDEF("file.txt")), "new write");
                TEST_RESULT_VOID(storagePutP(write, BUFSTRDEF("12345678901234567890")), "write");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("file missing");

                testRequestP(service, s3, HTTP_VERB_HEAD, "/BOGUS");
                testResponseP(service, .code = 404);

                TEST_RESULT_BOOL(storageExistsP(s3, STRDEF("BOGUS")), false, "check");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("info for / does not exist");

                TEST_RESULT_BOOL(storageInfoP(s3, NULL, .ignoreMissing = true).exists, false, "info for /");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("switch to service credentials");

                hrnServerScriptClose(service);

                #define TEST_SERVICE_ROLE                           "arn:aws:iam::123456789012:role/TestRole"
                #define TEST_SERVICE_TOKEN                          "TOKEN"
                #define TEST_SERVICE_URI                                                                                           \
                    "/?Action=AssumeRoleWithWebIdentity&RoleArn=arn%3Aaws%3Aiam%3A%3A123456789012%3Arole%2FTestRole"               \
                        "&RoleSessionName=pgBackRest&Version=2011-06-15&WebIdentityToken=" TEST_SERVICE_TOKEN
                #define TEST_SERVICE_RESPONSE                                                                                      \
                    "<AssumeRoleWithWebIdentityResponse xmlns=\"https://sts.amazonaws.com/doc/2011-06-15/\">\n"                    \
                    "  <AssumeRoleWithWebIdentityResult>\n"                                                                        \
                    "    <Credentials>\n"                                                                                          \
                    "      <SessionToken>zz</SessionToken>\n"                                                                      \
                    "      <SecretAccessKey>yy</SecretAccessKey>\n"                                                                \
                    "      <Expiration>%s</Expiration>\n"                                                                          \
                    "      <AccessKeyId>xx</AccessKeyId>\n"                                                                        \
                    "    </Credentials>\n"                                                                                         \
                    "  </AssumeRoleWithWebIdentityResult>\n"                                                                       \
                    "</AssumeRoleWithWebIdentityResponse>"

                HRN_STORAGE_PUT_Z(storagePosixNewP(TEST_PATH_STR, .write = true), "web-id-token", TEST_SERVICE_TOKEN);

                argList = strLstDup(commonArgList);
                hrnCfgArgRawFmt(argList, cfgOptRepoStorageHost, "%s:%u", strZ(host), port);
                hrnCfgArgRawStrId(argList, cfgOptRepoS3KeyType, storageS3KeyTypeWebId);
                HRN_CFG_LOAD(cfgCmdArchivePush, argList);

                TEST_ERROR(
                    storageRepoGet(0, true), OptionInvalidError,
                    "option 'repo1-s3-key-type' is 'web-id' but 'AWS_ROLE_ARN' and 'AWS_WEB_IDENTITY_TOKEN_FILE' are not set");

                setenv("AWS_ROLE_ARN", TEST_SERVICE_ROLE, true);

                TEST_ERROR(
                    storageRepoGet(0, true), OptionInvalidError,
                    "option 'repo1-s3-key-type' is 'web-id' but 'AWS_ROLE_ARN' and 'AWS_WEB_IDENTITY_TOKEN_FILE' are not set");

                setenv("AWS_WEB_IDENTITY_TOKEN_FILE", TEST_PATH "/web-id-token", true);

                s3 = storageRepoGet(0, true);
                driver = (StorageS3 *)storageDriver(s3);

                TEST_RESULT_STR_Z(driver->credRole, TEST_SERVICE_ROLE, "check role");
                TEST_RESULT_STR_Z(driver->webIdToken, TEST_SERVICE_TOKEN, "check token");

                // Set partSize to a small value for testing
                driver->partSize = 16;

                // Testing requires the auth http client to be redirected
                driver->credHost = hrnServerHost();
                driver->credHttpClient = httpClientNew(sckClientNew(host, authPort, 5000, 5000), 5000);

                hrnServerScriptAccept(service);

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("info for missing file");

                // Get service credentials
                hrnServerScriptAccept(auth);

                testRequestP(auth, NULL, HTTP_VERB_GET, TEST_SERVICE_URI);
                testResponseP(
                    auth,
                    .content = zNewFmt(TEST_SERVICE_RESPONSE, strZ(testS3DateTime(time(NULL) + (S3_CREDENTIAL_RENEW_SEC - 1)))));

                hrnServerScriptClose(auth);

                // File missing
                testRequestP(service, s3, HTTP_VERB_HEAD, "/BOGUS", .accessKey = "xx", .securityToken = "zz");
                testResponseP(service, .code = 404);

                TEST_RESULT_BOOL(storageInfoP(s3, STRDEF("BOGUS"), .ignoreMissing = true).exists, false, "file does not exist");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("info for file");

                // Get service credentials
                hrnServerScriptAccept(auth);

                testRequestP(auth, NULL, HTTP_VERB_GET, TEST_SERVICE_URI);
                testResponseP(
                    auth,
                    .content = zNewFmt(TEST_SERVICE_RESPONSE, strZ(testS3DateTime(time(NULL) + (S3_CREDENTIAL_RENEW_SEC * 2)))));

                hrnServerScriptClose(auth);

                testRequestP(service, s3, HTTP_VERB_HEAD, "/subdir/file1.txt");
                testResponseP(service, .header = "content-length:9999\r\nLast-Modified: Wed, 21 Oct 2015 07:28:00 GMT");

                StorageInfo info;
                TEST_ASSIGN(info, storageInfoP(s3, STRDEF("subdir/file1.txt")), "file exists");
                TEST_RESULT_BOOL(info.exists, true, "check exists");
                TEST_RESULT_UINT(info.type, storageTypeFile, "check type");
                TEST_RESULT_UINT(info.size, 9999, "check exists");
                TEST_RESULT_INT(info.timeModified, 1445412480, "check time");

                // Auth service no longer needed
                hrnServerScriptEnd(auth);

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("write zero-length file (without kms)");

                testRequestP(service, s3, HTTP_VERB_PUT, "/file.txt", .content = "");
                testResponseP(service);

                TEST_ASSIGN(write, storageNewWriteP(s3, STRDEF("file.txt")), "new write");
                TEST_RESULT_VOID(storagePutP(write, NULL), "write");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("info check existence only");

                testRequestP(service, s3, HTTP_VERB_HEAD, "/subdir/file2.txt");
                testResponseP(service, .header = "content-length:777\r\nLast-Modified: Wed, 22 Oct 2015 07:28:00 GMT");

                TEST_ASSIGN(info, storageInfoP(s3, STRDEF("subdir/file2.txt"), .level = storageInfoLevelExists), "file exists");
                TEST_RESULT_BOOL(info.exists, true, "check exists");
                TEST_RESULT_UINT(info.type, storageTypeFile, "check type");
                TEST_RESULT_UINT(info.size, 0, "check exists");
                TEST_RESULT_INT(info.timeModified, 0, "check time");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("errorOnMissing invalid because there are no paths");

                TEST_ERROR(
                    storageListP(s3, STRDEF("/"), .errorOnMissing = true), AssertError,
                    "assertion '!param.errorOnMissing || storageFeature(this, storageFeaturePath)' failed");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("error without xml");

                testRequestP(service, s3, HTTP_VERB_GET, "/?delimiter=%2F&list-type=2");
                testResponseP(service, .code = 344);

                TEST_ERROR(storageListP(s3, STRDEF("/")), ProtocolError,
                    "HTTP request failed with 344:\n"
                    "*** Path/Query ***:\n"
                    "GET /?delimiter=%2F&list-type=2\n"
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

                TEST_ERROR(storageListP(s3, STRDEF("/")), ProtocolError,
                    "HTTP request failed with 344:\n"
                    "*** Path/Query ***:\n"
                    "GET /?delimiter=%2F&list-type=2\n"
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
                        "    <IsTruncated>false</IsTruncated>"
                        "    <Contents>"
                        "        <Key>path/to/test_file</Key>"
                        "        <LastModified>2009-10-12T17:50:30.000Z</LastModified>"
                        "        <Size>787</Size>"
                        "    </Contents>"
                        "   <CommonPrefixes>"
                        "       <Prefix>path/to/test_path/</Prefix>"
                        "   </CommonPrefixes>"
                        "</ListBucketResult>");

                TEST_ERROR(
                    storageNewItrP(s3, STRDEF("/"), .errorOnMissing = true), AssertError,
                    "assertion '!param.errorOnMissing || storageFeature(this, storageFeaturePath)' failed");

                TEST_STORAGE_LIST(
                    s3, "/path/to",
                    "test_file {s=787, t=1255369830}\n"
                    "test_path/\n",
                    .level = storageInfoLevelBasic, .noRecurse = true);

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("list exists level");

                testRequestP(service, s3, HTTP_VERB_GET, "/?delimiter=%2F&list-type=2");
                testResponseP(
                    service,
                    .content =
                        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
                        "<ListBucketResult xmlns=\"http://s3.amazonaws.com/doc/2006-03-01/\">"
                        "    <IsTruncated>false</IsTruncated>"
                        "    <Contents>"
                        "        <Key>test1.txt</Key>"
                        "    </Contents>"
                        "   <CommonPrefixes>"
                        "       <Prefix>path1/</Prefix>"
                        "   </CommonPrefixes>"
                        "</ListBucketResult>");

                TEST_STORAGE_LIST(
                    s3, "/",
                    "path1/\n"
                    "test1.txt\n",
                    .noRecurse = true);

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("list a file in root with expression");

                testRequestP(service, s3, HTTP_VERB_GET, "/?delimiter=%2F&list-type=2&prefix=test");
                testResponseP(
                    service,
                    .content =
                        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
                        "<ListBucketResult xmlns=\"http://s3.amazonaws.com/doc/2006-03-01/\">"
                        "    <IsTruncated>false</IsTruncated>"
                        "    <Contents>"
                        "        <Key>test1.txt</Key>"
                        "    </Contents>"
                        "</ListBucketResult>");

                TEST_STORAGE_LIST(
                    s3, "/",
                    "test1.txt\n",
                    .noRecurse = true, .expression = "^test.*$");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("list files with continuation");

                testRequestP(service, s3, HTTP_VERB_GET, "/?delimiter=%2F&list-type=2&prefix=path%2Fto%2F");
                testResponseP(
                    service,
                    .content =
                        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
                        "<ListBucketResult xmlns=\"http://s3.amazonaws.com/doc/2006-03-01/\">"
                        "    <IsTruncated>true</IsTruncated>"
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
                        "    <IsTruncated>false</IsTruncated>"
                        "    <Contents>"
                        "        <Key>path/to/test3.txt</Key>"
                        "    </Contents>"
                        "   <CommonPrefixes>"
                        "       <Prefix>path/to/path2/</Prefix>"
                        "   </CommonPrefixes>"
                        "</ListBucketResult>");

                TEST_STORAGE_LIST(
                    s3, "/path/to",
                    "path1/\n"
                    "path2/\n"
                    "test1.txt\n"
                    "test2.txt\n"
                    "test3.txt\n",
                    .noRecurse = true);

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("list files with expression");

                testRequestP(service, s3, HTTP_VERB_GET, "/?delimiter=%2F&list-type=2&prefix=path%2Fto%2Ftest");
                testResponseP(
                    service,
                    .content =
                        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
                        "<ListBucketResult xmlns=\"http://s3.amazonaws.com/doc/2006-03-01/\">"
                        "    <IsTruncated>false</IsTruncated>"
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

                TEST_STORAGE_LIST(
                    s3, "/path/to",
                    "test1.path\n"
                    "test1.txt\n"
                    "test3.txt\n",
                    .level = storageInfoLevelExists, .noRecurse = true, .expression = "^test(1|3)");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("switch to path-style URIs");

                hrnServerScriptClose(service);

                argList = strLstDup(commonArgList);
                hrnCfgArgRawStrId(argList, cfgOptRepoS3UriStyle, storageS3UriStylePath);
                hrnCfgArgRawFmt(argList, cfgOptRepoStorageHost, "https://%s", strZ(host));
                hrnCfgArgRawFmt(argList, cfgOptRepoStoragePort, "%u", port);
                hrnCfgEnvRemoveRaw(cfgOptRepoS3Token);
                HRN_CFG_LOAD(cfgCmdArchivePush, argList);

                s3 = storageRepoGet(0, true);
                driver = (StorageS3 *)storageDriver(s3);

                // Set deleteMax to a small value for testing
                driver->deleteMax = 2;

                hrnServerScriptAccept(service);

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("error when no recurse because there are no paths");

                TEST_ERROR(
                    storagePathRemoveP(s3, STRDEF("/")), AssertError,
                    "assertion 'param.recurse || storageFeature(this, storageFeaturePath)' failed");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("remove files from root");

                testRequestP(service, s3, HTTP_VERB_GET, "/bucket/?list-type=2");
                testResponseP(
                    service,
                    .content =
                        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
                        "<ListBucketResult xmlns=\"http://s3.amazonaws.com/doc/2006-03-01/\">"
                        "    <IsTruncated>false</IsTruncated>"
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

                TEST_RESULT_VOID(storagePathRemoveP(s3, STRDEF("/"), .recurse = true), "remove");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("remove files in empty subpath (nothing to do)");

                testRequestP(service, s3, HTTP_VERB_GET, "/bucket/?list-type=2&prefix=path%2F");
                testResponseP(
                    service,
                    .content =
                        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
                        "<ListBucketResult xmlns=\"http://s3.amazonaws.com/doc/2006-03-01/\">"
                        "    <IsTruncated>false</IsTruncated>"
                        "</ListBucketResult>");

                TEST_RESULT_VOID(storagePathRemoveP(s3, STRDEF("/path"), .recurse = true), "remove");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("remove files with continuation");

                testRequestP(service, s3, HTTP_VERB_GET, "/bucket/?list-type=2&prefix=path%2Fto%2F");
                testResponseP(
                    service,
                    .content =
                        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
                        "<ListBucketResult xmlns=\"http://s3.amazonaws.com/doc/2006-03-01/\">"
                        "    <IsTruncated>true</IsTruncated>"
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
                        "    <IsTruncated>false</IsTruncated>"
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

                TEST_RESULT_VOID(storagePathRemoveP(s3, STRDEF("/path/to"), .recurse = true), "remove");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("path remove error and retry");

                testRequestP(service, s3, HTTP_VERB_GET, "/bucket/?list-type=2&prefix=path%2F");
                testResponseP(
                    service,
                    .content =
                        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
                        "<ListBucketResult xmlns=\"http://s3.amazonaws.com/doc/2006-03-01/\">"
                        "    <IsTruncated>false</IsTruncated>"
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
                            "<Error><Key>path/sample2.txt</Key><Code>AccessDenied</Code><Message>Access Denied</Message></Error>"
                            "</DeleteResult>");

                testRequestP(service, s3, HTTP_VERB_DELETE, "/bucket/path/sample2.txt");
                testResponseP(service, .code = 204);

                TEST_RESULT_VOID(storagePathRemoveP(s3, STRDEF("/path"), .recurse = true), "remove path");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("remove file");

                testRequestP(service, s3, HTTP_VERB_DELETE, "/bucket/path/to/test.txt");
                testResponseP(service, .code = 204);

                TEST_RESULT_VOID(storageRemoveP(s3, STRDEF("/path/to/test.txt")), "remove");

                // -----------------------------------------------------------------------------------------------------------------
                hrnServerScriptEnd(service);
            }
            HRN_FORK_PARENT_END();
        }
        HRN_FORK_END();
    }

    FUNCTION_HARNESS_RETURN_VOID();
}
