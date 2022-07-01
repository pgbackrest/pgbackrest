/***********************************************************************************************************************************
Test GCS Storage
***********************************************************************************************************************************/
#include "common/io/fdRead.h"
#include "common/io/fdWrite.h"
#include "storage/helper.h"

#include "common/harnessConfig.h"
#include "common/harnessFork.h"
#include "common/harnessServer.h"
#include "common/harnessStorage.h"

/***********************************************************************************************************************************
Constants
***********************************************************************************************************************************/
#define TEST_ENDPOINT                                               "storage.googleapis.com"
    STRING_STATIC(TEST_ENDPOINT_STR,                                TEST_ENDPOINT);
#define TEST_PORT                                                   ((unsigned int)443)
#define TEST_TIMEOUT                                                5000
#define TEST_CHUNK_SIZE                                             16
#define TEST_BUCKET                                                 "bucket"
    STRING_STATIC(TEST_BUCKET_STR,                                  TEST_BUCKET);
#define TEST_KEY_FILE                                               TEST_PATH "/key.json"
    STRING_STATIC(TEST_KEY_FILE_STR,                                TEST_KEY_FILE);
#define TEST_TOKEN                                                  "X X"
    STRING_STATIC(TEST_TOKEN_STR,                                   TEST_TOKEN);

#define TEST_KEY                                                                                                                   \
    "{\n"                                                                                                                          \
    "\"type\": \"service_account\",\n"                                                                                             \
    "\"project_id\": \"project\",\n"                                                                                               \
    "\"private_key\": "                                                                                                            \
        "\"-----BEGIN RSA PRIVATE KEY-----\\n"                                                                                     \
        "MIIEoQIBAAKCAQBV5Mryv79tXBKXfUWeSRHWmm+i5pGNgkMMRWiENGfqsrQxBFmR\\n"                                                      \
        "rPfVBcqfr5f7kKvOoUe772aAhJTiZNmeEPpN27Zn+0PsbDzXAS2BrEZUyynKaTDa\\n"                                                      \
        "kuw5MirBWVYWgIyjI0Y9airX3sSp9oxqrW0fpCxaApyYRAizzwQgcfw4ynhEp6Tn\\n"                                                      \
        "cBhVqkt3+lPB3I2Hhr/er+QmTll+xzKiqzZ2K8EXfiWprYMjttknL/+dgLWYF3I4\\n"                                                      \
        "weT5LxEohrzTgf3DyW3WIFulsqEGnNFRPGfwnPjZ0p//8zQ9vIvkCd/y7Tlz1Gvp\\n"                                                      \
        "BIPybgOgq/Xtac+HlvgRMWbDf5GmWHpKfG2zAgMBAAECggEAP/U9qcReJnCI54TA\\n"                                                      \
        "cjy2q7YTqplFiLmWc2y7hrX/KyQmSNmUWIUThevqFT4LTadMR3CQmcCJ8ujGdE3k\\n"                                                      \
        "PW8m8xLHoGXZDhMKuo6F9CjztfASDkaFujvs6ioQ7Cg5kkfmcROzGcgUXuniRyzv\\n"                                                      \
        "IgBBYW4+GEgZksgWMs3TpNU7mo1MaTFfvs69N6kZLvkTffNOQWPGnBLxwP/SFbn6\\n"                                                      \
        "4Z28s4xWXLyWuAvOUQtasAewgqdVpvh4ICpj5gTQiuFZ8pXKfzVj7OZEa5YkPawa\\n"                                                      \
        "cFxuHlr9VJmkimy3uepQFFlzEvq9hwjkv3gAWypOK8U5wXplkq2/nN08AdrqXh+O\\n"                                                      \
        "xr4yAQKBgQCVYOvdcnM0nFAwQHgO8PIwlKNEr2dGMDW+kVJ9+OHFn4dE3A0c4/9D\\n"                                                      \
        "LYlAZ4BdzikI1PDL6WaCODosanI8BQ2FQSb2eue7MZhvKa3cnCiPxLLeX5/Dxsy2\\n"                                                      \
        "sd5b8cfDbWH6xPzBium79a+CTI99W2iQG4JR4KfRn4RSsvu5mdGJSQKBgQCTM6aU\\n"                                                      \
        "rPrlXoB1vV9W0zrgSBJGToqEVzMyO3Ocg2I102fVJ7OTfdHN/sXKzEOFjb+gQ9UR\\n"                                                      \
        "89rx3BsMnOqCt1/WkxYchvtDh0SVIy9KgcHusX2D0TO8TowKL4eDuEsjK9RZXWRy\\n"                                                      \
        "FnERE9qofnrzwDsgIUcU7WgL1B+b/IsETuZbGwKBgEqZ/vG3aOXLcxjF+a+skafF\\n"                                                      \
        "c8yntPIOvaiQtxwGoeqqg0nWhA37p84K/dLWXft7LG8muaN8yx8ZqaPo/WgQNfJo\\n"                                                      \
        "2WabdwO7/x71N8lHi9JRL+ty4j1KGY7cQeq1U0i8ZgRxQLIACD9Asghm5/p5Hj1q\\n"                                                      \
        "H6l4gxdjjRgoHyNQOJ9RAoGAJytkVUWeUbCO4EWu++yjSxECg+DcbRDDF8fIIMq5\\n"                                                      \
        "NHd3trmkyEd/r5/uw+MMyJoKdrv2E5vfE+Ks8/NBV90xzGhBRwAIFlUFQ+Yj7GCQ\\n"                                                      \
        "2VIzgwMFEEOacg9psPw2Sjqce9clJlKgbZnp4lIdp4TsdsVEI+Z0fElKy/gsic3w\\n"                                                      \
        "8CcCgYB8cslQm0hr8hkNQoa2B1h/N6VnjegKRzz2xgS51t703ki3u9R3c5SjhW8j\\n"                                                      \
        "TuMS9w277P2ZBz38orc6lKbWg2GMrmsJfC9jgXkFi7NiZRdKl+DYcIMvNSZTR6j7\\n"                                                      \
        "DIo7CEz9TZW8QMM+VAt4pJTWo4Sy7iM2n0ZTFSGbBHboXHXnXQ==\\n"                                                                  \
        "-----END RSA PRIVATE KEY-----\\n\",\n"                                                                                    \
    "\"client_email\": \"service@project.iam.gserviceaccount.com\",\n"                                                             \
    "\"token_uri\": \"https://%s:%u/token\"\n"                                                                                     \
    "}\n"

/***********************************************************************************************************************************
Helper to build test requests
***********************************************************************************************************************************/
typedef struct TestRequestParam
{
    VAR_PARAM_HEADER;
    bool noBucket;
    bool upload;
    bool noAuth;
    const char *object;
    const char *query;
    const char *contentRange;
    const char *content;
    const char *range;
} TestRequestParam;

#define testRequestP(write, verb, ...)                                                                                             \
    testRequest(write, verb, (TestRequestParam){VAR_PARAM_INIT, __VA_ARGS__})

static void
testRequest(IoWrite *write, const char *verb, TestRequestParam param)
{
    String *request = strCatFmt(
        strNew(), "%s %s/storage/v1/b%s", verb, param.upload ? "/upload" : "", param.noBucket ? "" : "/bucket/o");

    // Add object
    if (param.object != NULL)
        strCatFmt(request, "/%s", strZ(httpUriEncode(STR(param.object), false)));

    // Add query
    if (param.query != NULL)
        strCatFmt(request, "?%s", param.query);

    // Add HTTP version and user agent
    strCatZ(request, " HTTP/1.1\r\nuser-agent:" PROJECT_NAME "/" PROJECT_VERSION "\r\n");

    // Add authorization string
    if (!param.noAuth)
        strCatZ(request, "authorization:X X\r\n");

    // Add content-length
    strCatFmt(request, "content-length:%zu\r\n", param.content == NULL ? 0 : strlen(param.content));

    // Add content-range
    if (param.contentRange != NULL)
        strCatFmt(request, "content-range:bytes %s\r\n", param.contentRange);

    // Add host
    strCatFmt(request, "host:%s\r\n", strZ(hrnServerHost()));

    // Add range
    if (param.range != NULL)
        strCatFmt(request, "range:bytes=%s\r\n", param.range);

    // Complete headers
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
    String *response = strCatFmt(strNew(), "HTTP/1.1 %u ", param.code);

    // Add reason for some codes
    switch (param.code)
    {
        case 200:
            strCatZ(response, "OK");
            break;

        case 403:
            strCatZ(response, "Forbidden");
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
Test Run
***********************************************************************************************************************************/
static void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    // Set storage helper
    static const StorageHelper storageHelperList[] = {STORAGE_GCS_HELPER, STORAGE_END_HELPER};
    storageHelperInit(storageHelperList);

    Storage *storageTest = storagePosixNewP(TEST_PATH_STR, .write = true);

    // Get test host and ports
    const String *const testHost = hrnServerHost();
    const unsigned int testPort = hrnServerPort(0);
    const unsigned int testPortAuth = hrnServerPort(1);
    const unsigned int testPortMeta = hrnServerPort(2);

    // *****************************************************************************************************************************
    if (testBegin("storageRepoGet()"))
    {
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("storage with default options");

        StringList *argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test");
        hrnCfgArgRawStrId(argList, cfgOptRepoType, STORAGE_GCS_TYPE);
        hrnCfgArgRawZ(argList, cfgOptRepoPath, "/repo");
        hrnCfgArgRawZ(argList, cfgOptRepoGcsBucket, TEST_BUCKET);
        hrnCfgArgRawStrId(argList, cfgOptRepoGcsKeyType, storageGcsKeyTypeToken);
        hrnCfgEnvRawZ(cfgOptRepoGcsKey, TEST_TOKEN);
        HRN_CFG_LOAD(cfgCmdArchivePush, argList);

        Storage *storage = NULL;
        TEST_ASSIGN(storage, storageRepoGet(0, false), "get repo storage");
        TEST_RESULT_STR_Z(storage->path, "/repo", "check path");
        TEST_RESULT_STR(((StorageGcs *)storageDriver(storage))->bucket, TEST_BUCKET_STR, "check bucket");
        TEST_RESULT_STR_Z(((StorageGcs *)storageDriver(storage))->endpoint, "storage.googleapis.com", "check endpoint");
        TEST_RESULT_UINT(((StorageGcs *)storageDriver(storage))->chunkSize, STORAGE_GCS_CHUNKSIZE_DEFAULT, "check chunk size");
        TEST_RESULT_STR(((StorageGcs *)storageDriver(storage))->token, TEST_TOKEN_STR, "check token");
        TEST_RESULT_BOOL(storageFeature(storage, storageFeaturePath), false, "check path feature");
    }

    // *****************************************************************************************************************************
    if (testBegin("storageGcsAuth*()"))
    {
        StorageGcs *storage = NULL;

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("jwt read-only");

        HRN_STORAGE_PUT(storageTest, TEST_KEY_FILE, BUFSTR(strNewFmt(TEST_KEY, "test.com", TEST_PORT)));

        TEST_ASSIGN(
            storage,
            (StorageGcs *)storageDriver(
                storageGcsNew(
                    STRDEF("/repo"), false, NULL, TEST_BUCKET_STR, storageGcsKeyTypeService, TEST_KEY_FILE_STR, TEST_CHUNK_SIZE,
                    TEST_ENDPOINT_STR, TEST_TIMEOUT, true, NULL, NULL)),
            "read-only gcs storage - service key");
        TEST_RESULT_STR_Z(httpUrlHost(storage->authUrl), "test.com", "check host");
        TEST_RESULT_STR_Z(httpUrlPath(storage->authUrl), "/token", "check path");
        TEST_RESULT_UINT(httpUrlPort(storage->authUrl), TEST_PORT, "check port");
        TEST_RESULT_UINT(httpUrlProtocolType(storage->authUrl), httpProtocolTypeHttps, "check protocol");

        TEST_RESULT_STR_Z(
            storageGcsAuthJwt(storage, 1613138142),
            "eyJhbGciOiJSUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzZXJ2aWNlQHByb2plY3QuaWFtLmdzZXJ2aWNlYWNjb3VudC5jb20iLCJzY29wZSI6Imh0d"
            "HBzOi8vd3d3Lmdvb2dsZWFwaXMuY29tL2F1dGgvZGV2c3RvcmFnZS5yZWFkX29ubHkiLCJhdWQiOiJodHRwczovL3Rlc3QuY29tOjQ0My90b2tlbiIsImV"
            "4cCI6MTYxMzE0MTc0MiwiaWF0IjoxNjEzMTM4MTQyfQ.IPfwPV_Qcd4_desFHlOc2wAdWQYUe7rTWG722J_lWNSu4vZH0YVE-9N5gjLZ6z_k8cnIOaenTc"
            "g-RX_vXmj0wZ4QyBF3t2mTVMM8jwDZtej2pPvyUslUJpcwyV6KNOOAO_TSxYN1OfE3hzMlhepC2GJRAft7oHKqaDV8DDXd4OulCM48OML0Y1ZPA-P1A-Ag"
            "5Sfkt1aq58teZurwY3ZtwKB5jbYnb8DHJHRJwSLZCXKDrfQrwlCIsXaWXSOxxge-L3B4yaywFtTyshhGj-e8takqxinOvrPpnhjdJzGJ7IvXky_MNbTey_"
            "RnpnhT0hjbftiJXEX6GyagwalzQRYdag",
            "jwt");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("jwt read/write");

        TEST_ASSIGN(
            storage,
            (StorageGcs *)storageDriver(
                storageGcsNew(
                    STRDEF("/repo"), true, NULL, TEST_BUCKET_STR, storageGcsKeyTypeService, TEST_KEY_FILE_STR, TEST_CHUNK_SIZE,
                    TEST_ENDPOINT_STR, TEST_TIMEOUT, true, NULL, NULL)),
            "read/write gcs storage - service key");

        TEST_RESULT_STR_Z(
            storageGcsAuthJwt(storage, 1613138142),
            "eyJhbGciOiJSUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzZXJ2aWNlQHByb2plY3QuaWFtLmdzZXJ2aWNlYWNjb3VudC5jb20iLCJzY29wZSI6Imh0d"
            "HBzOi8vd3d3Lmdvb2dsZWFwaXMuY29tL2F1dGgvZGV2c3RvcmFnZS5yZWFkX3dyaXRlIiwiYXVkIjoiaHR0cHM6Ly90ZXN0LmNvbTo0NDMvdG9rZW4iLCJ"
            "leHAiOjE2MTMxNDE3NDIsImlhdCI6MTYxMzEzODE0Mn0.RKyHLuaIS7Ut6aud9cK7E8SxmZBT8hlOLsXd3z5mC5Ieupkzm26LE6bMik0QCww-dPbkwJKnq"
            "Cb2cIO8GD2JdpXG4XkZhtCvbfZDZPkzioOlDwNA-Q7--btrgpFKL8C9FcZhJ1Tz24OGmIYdnZeeSf2hkBMuuIzrrve1BkRLaXfXUIWE519_tYaG4EpJ9nX"
            "N_ouEex5CJC-YnpyhqPeSG-DX7CalHdiOERIbzKGxdcEY3VcloQbWbgAqFMAUYBg6sHoNZbdHbwHQ62khvEeF4CI0MnYBYva3darYqmEEyaTfnzGEyyg62"
            "ocn6xBg6A6T4agO3xVT05EY-JWvq8Ockw",
            "jwt");
    }

    // *****************************************************************************************************************************
    if (testBegin("StorageGcs, StorageReadGcs, and StorageWriteGcs"))
    {
        HRN_STORAGE_PUT(storageTest, TEST_KEY_FILE, BUFSTR(strNewFmt(TEST_KEY, strZ(testHost), testPortAuth)));

        HRN_FORK_BEGIN()
        {
            HRN_FORK_CHILD_BEGIN(.prefix = "gcs server", .timeout = 5000)
            {
                TEST_RESULT_VOID(hrnServerRunP(HRN_FORK_CHILD_READ(), hrnServerProtocolTls, .port = testPort), "gcs server run");
            }
            HRN_FORK_CHILD_END();

            HRN_FORK_CHILD_BEGIN(.prefix = "auth server", .timeout = 5000)
            {
                TEST_RESULT_VOID(
                    hrnServerRunP(HRN_FORK_CHILD_READ(), hrnServerProtocolTls, .port = testPortAuth), "auth server run");
            }
            HRN_FORK_CHILD_END();

            HRN_FORK_CHILD_BEGIN(.prefix = "meta server", .timeout = 15000)
            {
                TEST_RESULT_VOID(
                    hrnServerRunP(HRN_FORK_CHILD_READ(), hrnServerProtocolSocket, .port = testPortMeta), "meta server run");
            }
            HRN_FORK_CHILD_END();

            HRN_FORK_PARENT_BEGIN()
            {
                // Do not use HRN_FORK_PARENT_WRITE() here so individual names can be assigned to help with debugging
                IoWrite *service = hrnServerScriptBegin(
                    ioFdWriteNewOpen(STRDEF("gcs client write"), HRN_FORK_PARENT_WRITE_FD(0), 2000));
                IoWrite *auth = hrnServerScriptBegin(
                    ioFdWriteNewOpen(STRDEF("auth client write"), HRN_FORK_PARENT_WRITE_FD(1), 2000));
                IoWrite *meta = hrnServerScriptBegin(
                    ioFdWriteNewOpen(STRDEF("meta client write"), HRN_FORK_PARENT_WRITE_FD(2), 2000));

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("test service auth");

                StringList *argList = strLstNew();
                hrnCfgArgRawZ(argList, cfgOptStanza, "test");
                hrnCfgArgRawStrId(argList, cfgOptRepoType, STORAGE_GCS_TYPE);
                hrnCfgArgRawZ(argList, cfgOptRepoPath, "/");
                hrnCfgArgRawZ(argList, cfgOptRepoGcsBucket, TEST_BUCKET);
                hrnCfgArgRawFmt(argList, cfgOptRepoGcsEndpoint, "%s:%u", strZ(hrnServerHost()), testPort);
                hrnCfgArgRawBool(argList, cfgOptRepoStorageVerifyTls, TEST_IN_CONTAINER);
                hrnCfgEnvRawZ(cfgOptRepoGcsKey, TEST_KEY_FILE);
                HRN_CFG_LOAD(cfgCmdArchivePush, argList);
                hrnCfgEnvRemoveRaw(cfgOptRepoGcsKey);

                Storage *storage = NULL;
                TEST_ASSIGN(storage, storageRepoGet(0, true), "get repo storage");

                // Generate the auth request. The JWT part will need to be ? since it can vary in content and size.
                const char *const preamble = "grant_type=urn%3Aietf%3Aparams%3Aoauth%3Agrant-type%3Ajwt-bearer&assertion=";
                const String *const jwt = storageGcsAuthJwt(((StorageGcs *)storageDriver(storage)), time(NULL));

                String *const authRequest = strCatFmt(
                    strNew(),
                    "POST /token HTTP/1.1\r\n"
                    "user-agent:" PROJECT_NAME "/" PROJECT_VERSION "\r\n"
                    "content-length:%zu\r\n"
                    "content-type:application/x-www-form-urlencoded\r\n"
                    "host:%s\r\n"
                    "\r\n"
                    "%s",
                    strSize(jwt) + strlen(preamble), strZ(hrnServerHost()), preamble);

                for (unsigned int jwtIdx = 0; jwtIdx < strSize(jwt); jwtIdx++)
                    strCatChr(authRequest, '?');

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("create bucket");

                hrnServerScriptAccept(auth);
                hrnServerScriptExpect(auth, authRequest);
                testResponseP(auth, .content = "{\"access_token\":\"X\",\"token_type\":\"X\",\"expires_in\":120}");
                hrnServerScriptClose(auth);

                hrnServerScriptAccept(service);
                testRequestP(service, HTTP_VERB_POST, .noBucket = true, .content = "{\"name\":\"bucket\"}");
                testResponseP(service);

                storageGcsRequestP(
                    (StorageGcs *)storageDriver(storage), HTTP_VERB_POST_STR, .noBucket = true,
                    .content = BUFSTR(
                        jsonWriteResult(
                            jsonWriteObjectEnd(
                                jsonWriteStr(
                                    jsonWriteKeyZ(jsonWriteObjectBegin(jsonWriteNewP()), GCS_JSON_NAME), STRDEF("bucket"))))));

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("auth error");

                // Authenticate again since the prior token timed out
                hrnServerScriptAccept(auth);
                hrnServerScriptExpect(auth, authRequest);
                testResponseP(auth, .content = "{\"error\":\"error\",\"error_description\":\"description\"}");
                hrnServerScriptClose(auth);

                TEST_ERROR(
                    storageGetP(storageNewReadP(storage, STRDEF("fi&le.txt"), .ignoreMissing = true)), ProtocolError,
                    "unable to get authentication token: [error] description");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("ignore missing file");

                // Authenticate again since the prior token errored out
                hrnServerScriptAccept(auth);
                hrnServerScriptExpect(auth, authRequest);
                testResponseP(auth, .content = "{\"access_token\":\"X\",\"token_type\":\"X\",\"expires_in\":7200}");
                hrnServerScriptClose(auth);

                // Auth service no longer needed
                hrnServerScriptEnd(auth);

                testRequestP(service, HTTP_VERB_GET, .object = "fi&le.txt", .query = "alt=media");
                testResponseP(service, .code = 404);

                TEST_RESULT_PTR(
                    storageGetP(storageNewReadP(storage, STRDEF("fi&le.txt"), .ignoreMissing = true)), NULL, "get file");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("error on missing file");

                testRequestP(service, HTTP_VERB_GET, .object = "file.txt", .query = "alt=media");
                testResponseP(service, .code = 404);

                TEST_ERROR(
                    storageGetP(storageNewReadP(storage, STRDEF("file.txt"))), FileMissingError,
                    "unable to open missing file '/file.txt' for read");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("get file with offset and limit");

                testRequestP(service, HTTP_VERB_GET, .object = "file.txt", .query = "alt=media", .range = "1-21");
                testResponseP(service, .content = "this is a sample file");

                TEST_RESULT_STR_Z(
                    strNewBuf(storageGetP(storageNewReadP(storage, STRDEF("file.txt"), .offset = 1, .limit = VARUINT64(21)))),
                    "this is a sample file", "get file");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("switch to auto auth");

                hrnServerScriptClose(service);

                StringList *argListAuto = strLstDup(argList);
                hrnCfgArgRawStrId(argListAuto, cfgOptRepoGcsKeyType, storageGcsKeyTypeAuto);
                HRN_CFG_LOAD(cfgCmdArchivePush, argListAuto);

                TEST_ASSIGN(storage, storageRepoGet(0, true), "get repo storage");

                // Replace the default authClient with one that points locally. The default host and url will still be used so they
                // can be verified when testing auth.
                ((StorageGcs *)storageDriver(storage))->authClient = httpClientNew(
                    sckClientNew(hrnServerHost(), testPortMeta, 2000, 2000), 2000);

                // Tests need the chunk size to be 16
                ((StorageGcs *)storageDriver(storage))->chunkSize = 16;

                hrnServerScriptAccept(service);

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("get zero-length file");

                // Get token automatically from metadata
                hrnServerScriptAccept(meta);
                hrnServerScriptExpectZ(
                    meta,
                    "GET /computeMetadata/v1/instance/service-accounts/default/token HTTP/1.1\r\n"
                    "user-agent:" PROJECT_NAME "/" PROJECT_VERSION "\r\n"
                    "content-length:0\r\n"
                    "host:metadata.google.internal\r\n"
                    "metadata-flavor:Google\r\n"
                    "\r\n");

                testResponseP(meta, .content = "{\"access_token\":\"X\",\"token_type\":\"X\",\"expires_in\":3600}");
                hrnServerScriptClose(meta);

                // Meta service no longer needed
                hrnServerScriptEnd(meta);

                testRequestP(service, HTTP_VERB_GET, .object = "file0.txt", .query = "alt=media");
                testResponseP(service);

                TEST_RESULT_STR_Z(
                    strNewBuf(storageGetP(storageNewReadP(storage, STRDEF("file0.txt")))), "", "get zero-length file");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("non-404 error");

                testRequestP(service, HTTP_VERB_GET, .object = "file.txt", .query = "alt=media");
                testResponseP(service, .code = 303, .content = "CONTENT");

                StorageRead *read = NULL;
                TEST_ASSIGN(read, storageNewReadP(storage, STRDEF("file.txt"), .ignoreMissing = true), "new read file");
                TEST_RESULT_BOOL(storageReadIgnoreMissing(read), true, "check ignore missing");
                TEST_RESULT_STR_Z(storageReadName(read), "/file.txt", "check name");

                TEST_ERROR_FMT(
                    ioReadOpen(storageReadIo(read)), ProtocolError,
                    "HTTP request failed with 303:\n"
                    "*** Path/Query ***:\n"
                    "GET /storage/v1/b/bucket/o/file.txt?alt=media\n"
                    "*** Request Headers ***:\n"
                    "authorization: <redacted>\n"
                    "content-length: 0\n"
                    "host: %s\n"
                    "*** Response Headers ***:\n"
                    "content-length: 7\n"
                    "*** Response Content ***:\n"
                    "CONTENT",
                    strZ(hrnServerHost()));

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("write error");

                testRequestP(
                    service, HTTP_VERB_POST, .query = "fields=md5Hash%2Csize&name=file.txt&uploadType=media", .upload = true,
                    .content = "ABCD");
                testResponseP(service, .code = 403);

                TEST_ERROR_FMT(
                    storagePutP(storageNewWriteP(storage, STRDEF("file.txt")), BUFSTRDEF("ABCD")), ProtocolError,
                    "HTTP request failed with 403 (Forbidden):\n"
                    "*** Path/Query ***:\n"
                    "POST /upload/storage/v1/b/bucket/o?fields=md5Hash%%2Csize&name=file.txt&uploadType=media\n"
                    "*** Request Headers ***:\n"
                    "authorization: <redacted>\n"
                    "content-length: 4\n"
                    "host: %s",
                    strZ(hrnServerHost()));

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("write file in one part (with retry)");

                testRequestP(
                    service, HTTP_VERB_POST, .query = "fields=md5Hash%2Csize&name=file.txt&uploadType=media", .upload = true,
                    .content = "ABCD");
                testResponseP(service, .code = 503);
                testRequestP(
                    service, HTTP_VERB_POST, .query = "fields=md5Hash%2Csize&name=file.txt&uploadType=media", .upload = true,
                    .content = "ABCD");
                testResponseP(service, .content = "{\"md5Hash\":\"ywjKSnu1+Wg8GRM6hIcspw==\"}");

                StorageWrite *write = NULL;
                TEST_ASSIGN(write, storageNewWriteP(storage, STRDEF("file.txt")), "new write");
                TEST_RESULT_VOID(storagePutP(write, BUFSTRDEF("ABCD")), "write");

                TEST_RESULT_BOOL(storageWriteAtomic(write), true, "write is atomic");
                TEST_RESULT_BOOL(storageWriteCreatePath(write), true, "path will be created");
                TEST_RESULT_UINT(storageWriteModeFile(write), 0, "file mode is 0");
                TEST_RESULT_UINT(storageWriteModePath(write), 0, "path mode is 0");
                TEST_RESULT_STR_Z(storageWriteName(write), "/file.txt", "check file name");
                TEST_RESULT_BOOL(storageWriteSyncFile(write), true, "file is synced");
                TEST_RESULT_BOOL(storageWriteSyncPath(write), true, "path is synced");

                TEST_RESULT_VOID(storageWriteGcsClose(write->driver), "close file again");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("write zero-length file");

                testRequestP(
                    service, HTTP_VERB_POST, .query = "fields=md5Hash%2Csize&name=file.txt&uploadType=media", .upload = true,
                    .content = "");
                testResponseP(service, .content = "{\"md5Hash\":\"1B2M2Y8AsgTpgAmY7PhCfg==\",\"size\":\"0\"}");

                TEST_ASSIGN(write, storageNewWriteP(storage, STRDEF("file.txt")), "new write");
                TEST_RESULT_VOID(storagePutP(write, NULL), "write");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("invalid md5");

                testRequestP(
                    service, HTTP_VERB_POST, .query = "fields=md5Hash%2Csize&name=file.txt&uploadType=media", .upload = true,
                    .content = "");
                testResponseP(service, .content = "{\"md5Hash\":\"ywjK\",\"size\":\"0\"}");

                TEST_ASSIGN(write, storageNewWriteP(storage, STRDEF("file.txt")), "new write");
                TEST_ERROR(
                    storagePutP(write, NULL), FormatError,
                    "expected md5 'd41d8cd98f00b204e9800998ecf8427e' for '/file.txt' but actual is 'cb08ca'");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("invalid size");

                testRequestP(
                    service, HTTP_VERB_POST, .query = "fields=md5Hash%2Csize&name=file.txt&uploadType=media", .upload = true,
                    .content = "");
                testResponseP(service, .content = "{\"md5Hash\":\"1B2M2Y8AsgTpgAmY7PhCfg==\",\"size\":\"55\"}");

                TEST_ASSIGN(write, storageNewWriteP(storage, STRDEF("file.txt")), "new write");
                TEST_ERROR(storagePutP(write, NULL), FormatError, "expected size 55 for '/file.txt' but actual is 0");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("write file in chunks with nothing left over on close");

                testRequestP(service, HTTP_VERB_POST, .upload = true, .query = "name=file.txt&uploadType=resumable");
                testResponseP(service, .header = "x-guploader-uploadid:ulid1");

                testRequestP(
                    service, HTTP_VERB_PUT, .upload = true, .noAuth = true,
                    .query = "name=file.txt&uploadType=resumable&upload_id=ulid1", .contentRange = "0-15/*",
                    .content = "1234567890123456");
                testResponseP(service, .code = 308);

                testRequestP(
                    service, HTTP_VERB_PUT, .upload = true, .noAuth = true,
                    .query = "fields=md5Hash%2Csize&name=file.txt&uploadType=resumable&upload_id=ulid1", .contentRange = "16-31/32",
                    .content = "7890123456789012");
                testResponseP(service, .content = "{\"md5Hash\":\"dnF5x6K/8ZZRzpfSlMMM+w==\",\"size\":\"32\"}");

                TEST_ASSIGN(write, storageNewWriteP(storage, STRDEF("file.txt")), "new write");
                TEST_RESULT_VOID(storagePutP(write, BUFSTRDEF("12345678901234567890123456789012")), "write");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("write file in chunks with something left over on close");

                testRequestP(service, HTTP_VERB_POST, .upload = true, .query = "name=file.txt&uploadType=resumable");
                testResponseP(service, .header = "x-guploader-uploadid:ulid2");

                testRequestP(
                    service, HTTP_VERB_PUT, .upload = true, .noAuth = true,
                    .query = "name=file.txt&uploadType=resumable&upload_id=ulid2", .contentRange = "0-15/*",
                    .content = "1234567890123456");
                testResponseP(service, .code = 503);
                testRequestP(
                    service, HTTP_VERB_PUT, .upload = true, .noAuth = true,
                    .query = "name=file.txt&uploadType=resumable&upload_id=ulid2", .contentRange = "0-15/*",
                    .content = "1234567890123456");
                testResponseP(service, .code = 308);

                testRequestP(
                    service, HTTP_VERB_PUT, .upload = true, .noAuth = true,
                    .query = "fields=md5Hash%2Csize&name=file.txt&uploadType=resumable&upload_id=ulid2", .contentRange = "16-19/20",
                    .content = "7890");
                testResponseP(service, .content = "{\"md5Hash\":\"/YXmLZvrRUKHcexohBiycQ==\",\"size\":\"20\"}");

                TEST_ASSIGN(write, storageNewWriteP(storage, STRDEF("file.txt")), "new write");
                TEST_RESULT_VOID(storagePutP(write, BUFSTRDEF("12345678901234567890")), "write");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("error on resumable upload (upload_id is redacted)");

                testRequestP(service, HTTP_VERB_POST, .upload = true, .query = "name=file.txt&uploadType=resumable");
                testResponseP(service, .header = "x-guploader-uploadid:ulid3");

                testRequestP(
                    service, HTTP_VERB_PUT, .upload = true, .noAuth = true,
                    .query = "name=file.txt&uploadType=resumable&upload_id=ulid3", .contentRange = "0-15/*",
                    .content = "1234567890123456");
                testResponseP(service, .code = 403);

                TEST_ASSIGN(write, storageNewWriteP(storage, STRDEF("file.txt")), "new write");

                TEST_ERROR_FMT(
                    storagePutP(storageNewWriteP(storage, STRDEF("file.txt")), BUFSTRDEF("12345678901234567")), ProtocolError,
                    "HTTP request failed with 403 (Forbidden):\n"
                    "*** Path/Query ***:\n"
                    "PUT /upload/storage/v1/b/bucket/o?name=file.txt&uploadType=resumable&upload_id=<redacted>\n"
                    "*** Request Headers ***:\n"
                    "content-length: 16\n"
                    "content-range: bytes 0-15/*\n"
                    "host: %s",
                    strZ(hrnServerHost()));

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("info for / does not exist");

                TEST_RESULT_BOOL(storageInfoP(storage, NULL, .ignoreMissing = true).exists, false, "info for /");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("info for missing file");

                testRequestP(service, HTTP_VERB_GET, .object = "BOGUS", .query = "fields=size%2Cupdated");
                testResponseP(service, .code = 404);

                TEST_RESULT_BOOL(
                    storageInfoP(storage, STRDEF("BOGUS"), .ignoreMissing = true).exists, false, "file does not exist");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("info for file");

                testRequestP(service, HTTP_VERB_GET, .object = "subdir/file1.txt", .query = "fields=size%2Cupdated");
                testResponseP(service, .content = "{\"size\":\"9999\",\"updated\":\"2015-10-21T07:28:00.000Z\"}");

                StorageInfo info;
                TEST_ASSIGN(info, storageInfoP(storage, STRDEF("subdir/file1.txt")), "file exists");
                TEST_RESULT_BOOL(info.exists, true, "check exists");
                TEST_RESULT_UINT(info.type, storageTypeFile, "check type");
                TEST_RESULT_UINT(info.size, 9999, "check exists");
                TEST_RESULT_INT(info.timeModified, 1445412480, "check time");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("info check existence only");

                testRequestP(service, HTTP_VERB_GET, .object = "subdir/file2.txt", .query = "fields=");
                testResponseP(service, .content = "{}");

                TEST_ASSIGN(
                    info, storageInfoP(storage, STRDEF("subdir/file2.txt"), .level = storageInfoLevelExists), "file exists");
                TEST_RESULT_BOOL(info.exists, true, "check exists");
                TEST_RESULT_UINT(info.type, storageTypeFile, "check type");
                TEST_RESULT_UINT(info.size, 0, "check exists");
                TEST_RESULT_INT(info.timeModified, 0, "check time");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("list basic level");

                testRequestP(
                    service, HTTP_VERB_GET,
                    .query =
                        "delimiter=%2F&fields=nextPageToken%2Cprefixes%2Citems%28name%2Csize%2Cupdated%29&prefix=path%2Fto%2F");
                testResponseP(
                    service,
                    .content =
                        "{"
                        "  \"prefixes\": ["
                        "     \"path/to/test_path/\""
                        "  ],"
                        "  \"items\": ["
                        "    {"
                        "      \"name\": \"path/to/test_file\","
                        "      \"size\": \"787\","
                        "      \"updated\": \"2009-10-12T17:50:30.123Z\""
                        "    }"
                        "  ]"
                        "}");

                TEST_ERROR(
                    storageNewItrP(storage, STRDEF("/"), .errorOnMissing = true), AssertError,
                    "assertion '!param.errorOnMissing || storageFeature(this, storageFeaturePath)' failed");

                TEST_STORAGE_LIST(
                    storage, "/path/to",
                    "test_file {s=787, t=1255369830}\n"
                    "test_path/\n",
                    .level = storageInfoLevelBasic, .noRecurse = true);

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("list exists level");

                testRequestP(service, HTTP_VERB_GET, .query = "delimiter=%2F&fields=nextPageToken%2Cprefixes%2Citems%28name%29");
                testResponseP(
                    service,
                    .content =
                        "{"
                        "  \"prefixes\": ["
                        "     \"path1/\""
                        "  ],"
                        "  \"items\": ["
                        "    {"
                        "      \"name\": \"test1.txt\""
                        "    }"
                        "  ]"
                        "}");

                TEST_STORAGE_LIST(
                    storage, "/",
                    "path1/\n"
                    "test1.txt\n",
                    .noRecurse = true);

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("list a file in root with expression");

                testRequestP(
                    service, HTTP_VERB_GET, .query = "delimiter=%2F&fields=nextPageToken%2Cprefixes%2Citems%28name%29&prefix=test");
                testResponseP(
                    service,
                    .content =
                        "{"
                        "  \"items\": ["
                        "    {"
                        "      \"name\": \"test1.txt\""
                        "    }"
                        "  ]"
                        "}");

                TEST_STORAGE_LIST(
                    storage, "/",
                    "test1.txt\n",
                    .noRecurse = true, .expression = "^test.*$");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("list files with continuation");

                testRequestP(
                    service, HTTP_VERB_GET,
                    .query = "delimiter=%2F&fields=nextPageToken%2Cprefixes%2Citems%28name%29&prefix=path%2Fto%2F");
                testResponseP(
                    service,
                    .content =
                        "{"
                        "  \"nextPageToken\": \"ueGx\","
                        "  \"prefixes\": ["
                        "     \"path/to/path1/\""
                        "  ],"
                        "  \"items\": ["
                        "    {"
                        "      \"name\": \"path/to/test1.txt\""
                        "    },"
                        "    {"
                        "      \"name\": \"path/to/test2.txt\""
                        "    }"
                        "  ]"
                        "}");

                testRequestP(
                    service, HTTP_VERB_GET,
                    .query = "delimiter=%2F&fields=nextPageToken%2Cprefixes%2Citems%28name%29&pageToken=ueGx&prefix=path%2Fto%2F");
                testResponseP(
                    service,
                    .content =
                        "{"
                        "  \"prefixes\": ["
                        "     \"path/to/path2/\""
                        "  ],"
                        "  \"items\": ["
                        "    {"
                        "      \"name\": \"path/to/test3.txt\""
                        "    }"
                        "  ]"
                        "}");

                TEST_STORAGE_LIST(
                    storage, "/path/to",
                    "path1/\n"
                    "path2/\n"
                    "test1.txt\n"
                    "test2.txt\n"
                    "test3.txt\n",
                    .noRecurse = true);

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("list files with expression");

                testRequestP(
                    service, HTTP_VERB_GET,
                    .query = "delimiter=%2F&fields=nextPageToken%2Cprefixes%2Citems%28name%29&prefix=path%2Fto%2Ftest");
                testResponseP(
                    service,
                    .content =
                        "{"
                        "  \"prefixes\": ["
                        "     \"path/to/test1.path/\","
                        "     \"path/to/test2.path/\""
                        "  ],"
                        "  \"items\": ["
                        "    {"
                        "      \"name\": \"path/to/test1.txt\""
                        "    },"
                        "    {"
                        "      \"name\": \"path/to/test2.txt\""
                        "    },"
                        "    {"
                        "      \"name\": \"path/to/test3.txt\""
                        "    }"
                        "  ]"
                        "}");

                TEST_STORAGE_LIST(
                    storage, "/path/to",
                    "test1.path\n"
                    "test1.txt\n"
                    "test3.txt\n",
                    .level = storageInfoLevelExists, .noRecurse = true, .expression = "^test(1|3)");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("switch to token auth");

                hrnServerScriptClose(service);

                hrnCfgArgRawZ(argList, cfgOptRepoGcsKeyType, "token");
                hrnCfgEnvRawZ(cfgOptRepoGcsKey, TEST_TOKEN);
                HRN_CFG_LOAD(cfgCmdArchivePush, argList);

                TEST_ASSIGN(storage, storageRepoGet(0, true), "get repo storage");

                hrnServerScriptAccept(service);

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("remove file");

                testRequestP(service, HTTP_VERB_DELETE, .object = "path/to/test.txt");
                testResponseP(service);

                TEST_RESULT_VOID(storageRemoveP(storage, STRDEF("/path/to/test.txt")), "remove");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("remove missing file");

                testRequestP(service, HTTP_VERB_DELETE, .object = "path/to/missing.txt");
                testResponseP(service, .code = 404);

                TEST_RESULT_VOID(storageRemoveP(storage, STRDEF("/path/to/missing.txt")), "remove");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("remove files from root");

                testRequestP(service, HTTP_VERB_GET, .query = "fields=nextPageToken%2Cprefixes%2Citems%28name%29");
                testResponseP(
                    service,
                    .content =
                        "{"
                        "  \"prefixes\": ["
                        "     \"not-deleted/\""
                        "  ],"
                        "  \"items\": ["
                        "    {"
                        "      \"name\": \"path/to/test1.txt\""
                        "    },"
                        "    {"
                        "      \"name\": \"path1/xxx.zzz\""
                        "    }"
                        "  ]"
                        "}");

                testRequestP(service, HTTP_VERB_DELETE, .object = "path/to/test1.txt");
                testResponseP(service);

                testRequestP(service, HTTP_VERB_DELETE, .object = "path1/xxx.zzz");
                testResponseP(service);

                TEST_RESULT_VOID(storagePathRemoveP(storage, STRDEF("/"), .recurse = true), "remove");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("remove files from path");

                testRequestP(service, HTTP_VERB_GET, .query = "fields=nextPageToken%2Cprefixes%2Citems%28name%29&prefix=path%2F");
                testResponseP(
                    service,
                    .content =
                        "{"
                        "  \"prefixes\": ["
                        "     \"path/not-deleted/\""
                        "  ],"
                        "  \"items\": ["
                        "    {"
                        "      \"name\": \"path/test1.txt\""
                        "    },"
                        "    {"
                        "      \"name\": \"path/path1/xxx.zzz\""
                        "    }"
                        "  ]"
                        "}");

                testRequestP(service, HTTP_VERB_DELETE, .object = "path/test1.txt");
                testResponseP(service);

                testRequestP(service, HTTP_VERB_DELETE, .object = "path/path1/xxx.zzz");
                testResponseP(service);

                TEST_RESULT_VOID(storagePathRemoveP(storage, STRDEF("/path"), .recurse = true), "remove");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("remove files in empty subpath (nothing to do)");

                testRequestP(service, HTTP_VERB_GET, .query = "fields=nextPageToken%2Cprefixes%2Citems%28name%29&prefix=path%2F");
                testResponseP(service, .content = "{}");

                TEST_RESULT_VOID(storagePathRemoveP(storage, STRDEF("/path"), .recurse = true), "remove");

                // -----------------------------------------------------------------------------------------------------------------
                hrnServerScriptEnd(service);
            }
            HRN_FORK_PARENT_END();
        }
        HRN_FORK_END();
    }

    FUNCTION_HARNESS_RETURN_VOID();
}
