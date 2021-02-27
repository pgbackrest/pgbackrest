/***********************************************************************************************************************************
Test GCS Storage
***********************************************************************************************************************************/
#include "common/io/fdRead.h"
#include "common/io/fdWrite.h"

#include "common/harnessConfig.h"
#include "common/harnessFork.h"
#include "common/harnessServer.h"
#include "common/harnessStorage.h"

/***********************************************************************************************************************************
Constants
***********************************************************************************************************************************/
#define TEST_ENDPOINT                                               "storage.googleapis.com"
    STRING_STATIC(TEST_ENDPOINT_STR,                                TEST_ENDPOINT);
#define TEST_PORT                                                   443
#define TEST_TIMEOUT                                                5000
#define TEST_CHUNK_SIZE                                             16
#define TEST_BUCKET                                                 "bucket"
    STRING_STATIC(TEST_BUCKET_STR,                                  TEST_BUCKET);
#define TEST_KEY_FILE                                               TEST_PATH "/key.json"
    STRING_STATIC(TEST_KEY_FILE_STR,                                TEST_KEY_FILE);
#define TEST_TOKEN                                                  "token"
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
// static StorageGcs *driver;
//
// typedef struct TestRequestParam
// {
//     VAR_PARAM_HEADER;
//     const char *content;
//     const char *blobType;
// } TestRequestParam;
//
// /*#define testRequestP(write, verb, path, ...)                                                                                       \*/
//     testRequest(write, verb, path, (TestRequestParam){VAR_PARAM_INIT, __VA_ARGS__})
//
// static void
// testRequest(IoWrite *write, const char *verb, const char *path, TestRequestParam param)
// {
//     String *request = strNewFmt("%s /" TEST_ACCOUNT "/" TEST_CONTAINER, verb);
//
//     // When SAS spit out the query and merge in the SAS key
//     if (driver->sasKey != NULL)
//     {
//         HttpQuery *query = httpQueryNewP();
//         StringList *pathQuery = strLstNewSplitZ(STR(path), "?");
//
//         if (strLstSize(pathQuery) == 2)
//             query = httpQueryNewStr(strLstGet(pathQuery, 1));
//
//         httpQueryMerge(query, driver->sasKey);
//
//         strCat(request, strLstGet(pathQuery, 0));
//         strCatZ(request, "?");
//         strCat(request, httpQueryRenderP(query));
//     }
//     // Else just output path as is
//     else
//         strCatZ(request, path);
//
//     // Add HTTP version and user agent
//     strCatZ(request, " HTTP/1.1\r\nuser-agent:" PROJECT_NAME "/" PROJECT_VERSION "\r\n");
//
//     // Add authorization string
//     if (driver->sharedKey != NULL)
//         strCatZ(request, "authorization:SharedKey account:????????????????????????????????????????????\r\n");
//
//     // Add content-length
//     strCatFmt(request, "content-length:%zu\r\n", param.content == NULL ? 0 : strlen(param.content));
//
//     // Add md5
//     if (param.content != NULL)
//     {
//            strCatFmt(
//                request, "content-md5:%s\r\n",
//                strZ(strNewEncode(encodeBase64, cryptoHashOne(HASH_TYPE_MD5_STR, BUFSTRZ(param.content)))));
//     }
//
//     // Add date
//     if (driver->sharedKey != NULL)
//         strCatZ(request, "date:???, ?? ??? ???? ??:??:?? GMT\r\n");
//
//     // Add host
//     strCatFmt(request, "host:%s\r\n", strZ(hrnServerHost()));
//
//     // Add blob type
//     if (param.blobType != NULL)
//         strCatFmt(request, "x-ms-blob-type:%s\r\n", param.blobType);
//
//     // Add version
//     if (driver->sharedKey != NULL)
//         strCatZ(request, "x-ms-version:2019-02-02\r\n");
//
//     // Complete headers
//     strCatZ(request, "\r\n");
//
//     // Add content
//     if (param.content != NULL)
//         strCatZ(request, param.content);
//
//     hrnServerScriptExpect(write, request);
// }

/***********************************************************************************************************************************
Helper to build test responses
***********************************************************************************************************************************/
// typedef struct TestResponseParam
// {
//     VAR_PARAM_HEADER;
//     unsigned int code;
//     const char *header;
//     const char *content;
// } TestResponseParam;
//
// /*#define testResponseP(write, ...)                                                                                                  \*/
//     testResponse(write, (TestResponseParam){VAR_PARAM_INIT, __VA_ARGS__})
//
// static void
// testResponse(IoWrite *write, TestResponseParam param)
// {
//     // Set code to 200 if not specified
//     param.code = param.code == 0 ? 200 : param.code;
//
//     // Output header and code
//     String *response = strNewFmt("HTTP/1.1 %u ", param.code);
//
//     // Add reason for some codes
//     switch (param.code)
//     {
//         case 200:
//             strCatZ(response, "OK");
//             break;
//
//         case 403:
//             strCatZ(response, "Forbidden");
//             break;
//     }
//
//     // End header
//     strCatZ(response, "\r\n");
//
//     // Headers
//     if (param.header != NULL)
//         strCatFmt(response, "%s\r\n", param.header);
//
//     // Content
//     if (param.content != NULL)
//     {
//         strCatFmt(
//             response,
//             "content-length:%zu\r\n"
//                 "\r\n"
//                 "%s",
//             strlen(param.content), param.content);
//     }
//     else
//         strCatZ(response, "\r\n");
//
//     hrnServerScriptReply(write, response);
// }

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    Storage *storageTest = storagePosixNewP(strNew(testPath()), .write = true);

    // Get test host and ports
    const String *const testHost = hrnServerHost();
    const unsigned int testPort = hrnServerPort(0);
    const unsigned int testPortAuth = hrnServerPort(1);

    // Generate test key with local auth host/port
    const Buffer *const testKey = BUFSTR(strNewFmt(TEST_KEY, strZ(testHost), testPortAuth));

    // *****************************************************************************************************************************
    if (testBegin("storageRepoGet()"))
    {
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("storage with default options");

        StringList *argList = strLstNew();
        strLstAddZ(argList, "--" CFGOPT_STANZA "=test");
        hrnCfgArgRawZ(argList, cfgOptRepoType, STORAGE_GCS_TYPE);
        hrnCfgArgRawZ(argList, cfgOptRepoPath, "/repo");
        hrnCfgArgRawZ(argList, cfgOptRepoGcsBucket, TEST_BUCKET);
        hrnCfgArgRawZ(argList, cfgOptRepoGcsKeyType, STORAGE_GCS_KEY_TYPE_TOKEN);
        hrnCfgArgRawZ(argList, cfgOptRepoGcsKey, TEST_TOKEN);
        harnessCfgLoad(cfgCmdArchivePush, argList);

        Storage *storage = NULL;
        TEST_ASSIGN(storage, storageRepoGet(0, false), "get repo storage");
        TEST_RESULT_STR_Z(storage->path, "/repo", "    check path");
        TEST_RESULT_STR(((StorageGcs *)storage->driver)->bucket, TEST_BUCKET_STR, "    check bucket");
        TEST_RESULT_STR_Z(((StorageGcs *)storage->driver)->endpoint, "storage.googleapis.com", "    check endpoint");
        TEST_RESULT_UINT(((StorageGcs *)storage->driver)->chunkSize, STORAGE_GCS_CHUNKSIZE_DEFAULT, "    check chunk size");
        TEST_RESULT_STR(((StorageGcs *)storage->driver)->token, TEST_TOKEN_STR, "    check token");
        TEST_RESULT_BOOL(storageFeature(storage, storageFeaturePath), false, "    check path feature");
        TEST_RESULT_BOOL(storageFeature(storage, storageFeatureCompress), false, "    check compress feature");
    }

    // *****************************************************************************************************************************
    if (testBegin("storageGcsAuth*()"))
    {
        StorageGcs *storage = NULL;

        // !!! HACKY WAY TO GET A BEARER TOKEN FOR TESTING AT THE COMMAND LINE
        // TEST_RESULT_STR_Z(
        //     storageGcsAuthToken(
        //         (StorageGcs *)storageDriver(
        //             storageGcsNew(
        //                 STRDEF("/repo"), true, NULL, TEST_BUCKET_STR, storageGcsKeyTypeService,
        //                 strNewFmt("/home/%s/pgbackrest/test/scratch.gcs.json", testUser()), TEST_CHUNK_SIZE, TEST_ENDPOINT_STR,
        //                 TEST_PORT, TEST_TIMEOUT, true, NULL, NULL)), time(NULL)).token,
        //     "", "authentication token");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("jwt read-only");

        HRN_STORAGE_PUT(storageTest, TEST_KEY_FILE, testKey);

        TEST_ASSIGN(
            storage,
            (StorageGcs *)storageDriver(
                storageGcsNew(
                    STRDEF("/repo"), false, NULL, TEST_BUCKET_STR, storageGcsKeyTypeService, TEST_KEY_FILE_STR, TEST_CHUNK_SIZE,
                    TEST_ENDPOINT_STR, TEST_PORT, TEST_TIMEOUT, true, NULL, NULL)),
            "read-only gcs storage - service key");

        TEST_RESULT_STR_Z(
            storageGcsAuthJwt(storage, 1613138142),
            "eyJhbGciOiJSUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzZXJ2aWNlQHByb2plY3QuaWFtLmdzZXJ2aWNlYWNjb3VudC5jb20iLCJzY29wZSI6Imh0d"
            "HBzOi8vd3d3Lmdvb2dsZWFwaXMuY29tL2F1dGgvZGV2c3RvcmFnZS5yZWFkX29ubHkiLCJhdWQiOiJodHRwczovL29hdXRoMi5nb29nbGVhcGlzLmNvbS9"
            "0b2tlbiIsImV4cCI6MTYxMzE0MTc0MiwiaWF0IjoxNjEzMTM4MTQyfQ.CphPa6w6AzaQ61sO2B0EO8zbCk_wBMcxiOyTZPcNnEIb7SpbAe5dM7UCbK_Hcd"
            "F0VTASP3h06vhPhAcJCn0gvyAnzThg0nJddCgFUnYjMqmLjSNkI-yteQwsOpYEMletE73c9dnMYawIjqGSUKND9d_q8fjtYlpAXjczu5jmSax3YMAHacWZ"
            "GMauR0oMiDbIgqFgAcAuneSYbFQW-hKrQ30DlRkbcsvHxpAOxnUfzUo65OC4wSzUaCAy85LzrxWEjTtaI6L6YrvP0qyICe__uyV-uvKaOUBvM_pY4uJEqD"
            "MP8S2uA2E76Tf2MJ_NcIYV0xR7XhqlmTSkc4YqZKBFoQ",
            "jwt");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("jwt read/write");

        HRN_STORAGE_PUT_Z(storageTest, TEST_KEY_FILE, TEST_KEY);

        TEST_ASSIGN(
            storage,
            (StorageGcs *)storageDriver(
                storageGcsNew(
                    STRDEF("/repo"), true, NULL, TEST_BUCKET_STR, storageGcsKeyTypeService, TEST_KEY_FILE_STR, TEST_CHUNK_SIZE,
                    TEST_ENDPOINT_STR, TEST_PORT, TEST_TIMEOUT, true, NULL, NULL)),
            "read/write gcs storage - service key");

        TEST_RESULT_STR_Z(
            storageGcsAuthJwt(storage, 1613138142),
            "eyJhbGciOiJSUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzZXJ2aWNlQHByb2plY3QuaWFtLmdzZXJ2aWNlYWNjb3VudC5jb20iLCJzY29wZSI6Imh0d"
            "HBzOi8vd3d3Lmdvb2dsZWFwaXMuY29tL2F1dGgvZGV2c3RvcmFnZS5yZWFkX3dyaXRlIiwiYXVkIjoiaHR0cHM6Ly9vYXV0aDIuZ29vZ2xlYXBpcy5jb20"
            "vdG9rZW4iLCJleHAiOjE2MTMxNDE3NDIsImlhdCI6MTYxMzEzODE0Mn0.KGDLo5EXM4__7b8dBYKjOJMNAJau02i6quQn4ZqJ_3YBU6ruMV66yY2YjukGC"
            "0BSxDPcYBLsKK7TnDPrJRGLXqoS50X45BAGa5gCApReSjieO3BKgH_Eqf9vOauBgjzXVugj5PP3dw886_VeuQqt4pNAEZruj2qBTKCnrYcYM9i5UV-A0Cl"
            "pov7dTnhGu7ROkFlJ62n_Skl-v48DEVjWGmLyAVE30dIu0niWHvybG9pIfENHlnF1fWUhIbIGm4dFmcJBlNpTBv8cIhA0Y_zriEUAN3hMqFfFXMPjRUqOU"
            "n1xbG88ranOuk9XS4CRrzH4Cv1wgjO4Wdk_S-8poZ6N1A",
            "jwt");
    }

    // *****************************************************************************************************************************
    if (testBegin("StorageGcs, StorageReadGcs, and StorageWriteGcs"))
    {
        HARNESS_FORK_BEGIN()
        {
            HARNESS_FORK_CHILD_BEGIN(0, true)
            {
                TEST_RESULT_VOID(
                    hrnServerRunP(
                        ioFdReadNew(strNew("gcs server read"), HARNESS_FORK_CHILD_READ(), 5000), hrnServerProtocolTls,
                        .port = testPort),
                    "gcs server run");
            }
            HARNESS_FORK_CHILD_END();

            HARNESS_FORK_CHILD_BEGIN(0, true)
            {
                TEST_RESULT_VOID(
                    hrnServerRunP(
                        ioFdReadNew(strNew("auth server read"), HARNESS_FORK_CHILD_READ(), 5000), hrnServerProtocolSocket,
                        .port = testPortAuth),
                    "auth server run");
            }
            HARNESS_FORK_CHILD_END();

            HARNESS_FORK_PARENT_BEGIN()
            {
                IoWrite *service = hrnServerScriptBegin(
                    ioFdWriteNew(strNew("gcs client write"), HARNESS_FORK_PARENT_WRITE_PROCESS(0), 2000));
                IoWrite *auth = hrnServerScriptBegin(
                    ioFdWriteNew(strNew("auth client write"), HARNESS_FORK_PARENT_WRITE_PROCESS(1), 2000));

                // // -----------------------------------------------------------------------------------------------------------------
                // TEST_TITLE("test against local host");
                //
                // StringList *argList = strLstNew();
                // strLstAddZ(argList, "--" CFGOPT_STANZA "=test");
                // hrnCfgArgRawZ(argList, cfgOptRepoType, STORAGE_GCS_TYPE);
                // hrnCfgArgRawZ(argList, cfgOptRepoPath, "/");
                // hrnCfgArgRawZ(argList, cfgOptRepoGcsContainer, TEST_CONTAINER);
                // hrnCfgArgRaw(argList, cfgOptRepoGcsHost, hrnServerHost());
                // hrnCfgArgRawFmt(argList, cfgOptRepoGcsPort, "%u", hrnServerPort(0));
                // hrnCfgArgRawBool(argList, cfgOptRepoGcsVerifyTls, testContainer());
                // hrnCfgEnvRawZ(cfgOptRepoGcsAccount, TEST_ACCOUNT);
                // hrnCfgEnvRawZ(cfgOptRepoGcsKey, TEST_KEY_SHARED);
                // harnessCfgLoad(cfgCmdArchivePush, argList);
                //
                // Storage *storage = NULL;
                // TEST_ASSIGN(storage, storageRepoGet(0, true), "get repo storage");
                //
                // driver = (StorageGcs *)storage->driver;
                // TEST_RESULT_STR(driver->host, hrnServerHost(), "    check host");
                // TEST_RESULT_STR_Z(driver->pathPrefix,  "/" TEST_ACCOUNT "/" TEST_CONTAINER, "    check path prefix");
                // TEST_RESULT_BOOL(driver->fileId == 0, false, "    check file id");
                //
                // // Tests need the chunk size to be 16
                // driver->chunkSize = 16;
                //
                // // -----------------------------------------------------------------------------------------------------------------
                // TEST_TITLE("ignore missing file");
                //
                // hrnServerScriptAccept(service);
                // testRequestP(service, HTTP_VERB_GET, "/fi%26le.txt");
                // testResponseP(service, .code = 404);
                //
                // TEST_RESULT_PTR(
                //     storageGetP(storageNewReadP(storage, strNew("fi&le.txt"), .ignoreMissing = true)), NULL, "get file");
                //
                // // -----------------------------------------------------------------------------------------------------------------
                // TEST_TITLE("error on missing file");
                //
                // testRequestP(service, HTTP_VERB_GET, "/file.txt");
                // testResponseP(service, .code = 404);
                //
                // TEST_ERROR(
                //     storageGetP(storageNewReadP(storage, strNew("file.txt"))), FileMissingError,
                //     "unable to open '/file.txt': No such file or directory");
                //
                // // -----------------------------------------------------------------------------------------------------------------
                // TEST_TITLE("get file");
                //
                // testRequestP(service, HTTP_VERB_GET, "/file.txt");
                // testResponseP(service, .content = "this is a sample file");
                //
                // TEST_RESULT_STR_Z(
                //     strNewBuf(storageGetP(storageNewReadP(storage, strNew("file.txt")))), "this is a sample file", "get file");
                //
                // // -----------------------------------------------------------------------------------------------------------------
                // TEST_TITLE("get zero-length file");
                //
                // testRequestP(service, HTTP_VERB_GET, "/file0.txt");
                // testResponseP(service);
                //
                // TEST_RESULT_STR_Z(
                //     strNewBuf(storageGetP(storageNewReadP(storage, strNew("file0.txt")))), "", "get zero-length file");
                //
                // // -----------------------------------------------------------------------------------------------------------------
                // TEST_TITLE("non-404 error");
                //
                // testRequestP(service, HTTP_VERB_GET, "/file.txt");
                // testResponseP(service, .code = 303, .content = "CONTENT");
                //
                // StorageRead *read = NULL;
                // TEST_ASSIGN(read, storageNewReadP(storage, strNew("file.txt"), .ignoreMissing = true), "new read file");
                // TEST_RESULT_BOOL(storageReadIgnoreMissing(read), true, "    check ignore missing");
                // TEST_RESULT_STR_Z(storageReadName(read), "/file.txt", "    check name");
                //
                // TEST_ERROR_FMT(
                //     ioReadOpen(storageReadIo(read)), ProtocolError,
                //     "HTTP request failed with 303:\n"
                //     "*** Path/Query ***:\n"
                //     "/account/container/file.txt\n"
                //     "*** Request Headers ***:\n"
                //     "authorization: <redacted>\n"
                //     "content-length: 0\n"
                //     "date: <redacted>\n"
                //     "host: %s\n"
                //     "x-ms-version: 2019-02-02\n"
                //     "*** Response Headers ***:\n"
                //     "content-length: 7\n"
                //     "*** Response Content ***:\n"
                //     "CONTENT",
                //     strZ(hrnServerHost()));
                //
                // // -----------------------------------------------------------------------------------------------------------------
                // TEST_TITLE("write error");
                //
                // testRequestP(service, HTTP_VERB_PUT, "/file.txt", .blobType = "BlockBlob", .content = "ABCD");
                // testResponseP(service, .code = 403);
                //
                // TEST_ERROR_FMT(
                //     storagePutP(storageNewWriteP(storage, strNew("file.txt")), BUFSTRDEF("ABCD")), ProtocolError,
                //     "HTTP request failed with 403 (Forbidden):\n"
                //     "*** Path/Query ***:\n"
                //     "/account/container/file.txt\n"
                //     "*** Request Headers ***:\n"
                //     "authorization: <redacted>\n"
                //     "content-length: 4\n"
                //     "content-md5: ywjKSnu1+Wg8GRM6hIcspw==\n"
                //     "date: <redacted>\n"
                //     "host: %s\n"
                //     "x-ms-blob-type: BlockBlob\n"
                //     "x-ms-version: 2019-02-02",
                //     strZ(hrnServerHost()));
                //
                // // -----------------------------------------------------------------------------------------------------------------
                // TEST_TITLE("write file in one part (with retry)");
                //
                // testRequestP(service, HTTP_VERB_PUT, "/file.txt", .blobType = "BlockBlob", .content = "ABCD");
                // testResponseP(service, .code = 503);
                // testRequestP(service, HTTP_VERB_PUT, "/file.txt", .blobType = "BlockBlob", .content = "ABCD");
                // testResponseP(service);
                //
                // StorageWrite *write = NULL;
                // TEST_ASSIGN(write, storageNewWriteP(storage, strNew("file.txt")), "new write");
                // TEST_RESULT_VOID(storagePutP(write, BUFSTRDEF("ABCD")), "write");
                //
                // TEST_RESULT_BOOL(storageWriteAtomic(write), true, "write is atomic");
                // TEST_RESULT_BOOL(storageWriteCreatePath(write), true, "path will be created");
                // TEST_RESULT_UINT(storageWriteModeFile(write), 0, "file mode is 0");
                // TEST_RESULT_UINT(storageWriteModePath(write), 0, "path mode is 0");
                // TEST_RESULT_STR_Z(storageWriteName(write), "/file.txt", "check file name");
                // TEST_RESULT_BOOL(storageWriteSyncFile(write), true, "file is synced");
                // TEST_RESULT_BOOL(storageWriteSyncPath(write), true, "path is synced");
                //
                // TEST_RESULT_VOID(storageWriteGcsClose(write->driver), "close file again");
                //
                // // -----------------------------------------------------------------------------------------------------------------
                // TEST_TITLE("write zero-length file");
                //
                // testRequestP(service, HTTP_VERB_PUT, "/file.txt", .blobType = "BlockBlob", .content = "");
                // testResponseP(service);
                //
                // TEST_ASSIGN(write, storageNewWriteP(storage, strNew("file.txt")), "new write");
                // TEST_RESULT_VOID(storagePutP(write, NULL), "write");
                //
                // // -----------------------------------------------------------------------------------------------------------------
                // TEST_TITLE("write file in chunks with nothing left over on close");
                //
                // testRequestP(
                //     service, HTTP_VERB_PUT, "/file.txt?chunkid=0AAAAAAACCCCCCCCx0000000&comp=chunk", .content = "1234567890123456");
                // testResponseP(service);
                //
                // testRequestP(
                //     service, HTTP_VERB_PUT, "/file.txt?chunkid=0AAAAAAACCCCCCCCx0000001&comp=chunk", .content = "7890123456789012");
                // testResponseP(service);
                //
                // testRequestP(
                //     service, HTTP_VERB_PUT, "/file.txt?comp=chunklist",
                //     .content =
                //         "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                //         "<BlockList>"
                //         "<Uncommitted>0AAAAAAACCCCCCCCx0000000</Uncommitted>"
                //         "<Uncommitted>0AAAAAAACCCCCCCCx0000001</Uncommitted>"
                //         "</BlockList>\n");
                // testResponseP(service);
                //
                // // Test needs a predictable file id
                // driver->fileId = 0x0AAAAAAACCCCCCCC;
                //
                // TEST_ASSIGN(write, storageNewWriteP(storage, strNew("file.txt")), "new write");
                // TEST_RESULT_VOID(storagePutP(write, BUFSTRDEF("12345678901234567890123456789012")), "write");
                //
                // // -----------------------------------------------------------------------------------------------------------------
                // TEST_TITLE("write file in chunks with something left over on close");
                //
                // testRequestP(
                //     service, HTTP_VERB_PUT, "/file.txt?chunkid=0AAAAAAACCCCCCCDx0000000&comp=chunk", .content = "1234567890123456");
                // testResponseP(service);
                //
                // testRequestP(
                //     service, HTTP_VERB_PUT, "/file.txt?chunkid=0AAAAAAACCCCCCCDx0000001&comp=chunk", .content = "7890");
                // testResponseP(service);
                //
                // testRequestP(
                //     service, HTTP_VERB_PUT, "/file.txt?comp=chunklist",
                //     .content =
                //         "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                //         "<BlockList>"
                //         "<Uncommitted>0AAAAAAACCCCCCCDx0000000</Uncommitted>"
                //         "<Uncommitted>0AAAAAAACCCCCCCDx0000001</Uncommitted>"
                //         "</BlockList>\n");
                // testResponseP(service);
                //
                // TEST_ASSIGN(write, storageNewWriteP(storage, strNew("file.txt")), "new write");
                // TEST_RESULT_VOID(storagePutP(write, BUFSTRDEF("12345678901234567890")), "write");
                //
                // // -----------------------------------------------------------------------------------------------------------------
                // TEST_TITLE("info for missing file");
                //
                // testRequestP(service, HTTP_VERB_HEAD, "/BOGUS");
                // testResponseP(service, .code = 404);
                //
                // TEST_RESULT_BOOL(
                //     storageInfoP(storage, strNew("BOGUS"), .ignoreMissing = true).exists, false, "file does not exist");
                //
                // // -----------------------------------------------------------------------------------------------------------------
                // TEST_TITLE("info for file");
                //
                // testRequestP(service, HTTP_VERB_HEAD, "/subdir/file1.txt");
                // testResponseP(service, .header = "content-length:9999\r\nLast-Modified: Wed, 21 Oct 2015 07:28:00 GMT");
                //
                // StorageInfo info;
                // TEST_ASSIGN(info, storageInfoP(storage, strNew("subdir/file1.txt")), "file exists");
                // TEST_RESULT_BOOL(info.exists, true, "    check exists");
                // TEST_RESULT_UINT(info.type, storageTypeFile, "    check type");
                // TEST_RESULT_UINT(info.size, 9999, "    check exists");
                // TEST_RESULT_INT(info.timeModified, 1445412480, "    check time");
                //
                // // -----------------------------------------------------------------------------------------------------------------
                // TEST_TITLE("info check existence only");
                //
                // testRequestP(service, HTTP_VERB_HEAD, "/subdir/file2.txt");
                // testResponseP(service, .header = "content-length:777\r\nLast-Modified: Wed, 22 Oct 2015 07:28:00 GMT");
                //
                // TEST_ASSIGN(
                //     info, storageInfoP(storage, strNew("subdir/file2.txt"), .level = storageInfoLevelExists), "file exists");
                // TEST_RESULT_BOOL(info.exists, true, "    check exists");
                // TEST_RESULT_UINT(info.type, storageTypeFile, "    check type");
                // TEST_RESULT_UINT(info.size, 0, "    check exists");
                // TEST_RESULT_INT(info.timeModified, 0, "    check time");
                //
                // // -----------------------------------------------------------------------------------------------------------------
                // TEST_TITLE("list basic level");
                //
                // testRequestP(service, HTTP_VERB_GET, "?comp=list&delimiter=%2F&prefix=path%2Fto%2F&restype=container");
                // testResponseP(
                //     service,
                //     .content =
                //         "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
                //         "<EnumerationResults>"
                //         "    <Blobs>"
                //         "        <Blob>"
                //         "            <Name>path/to/test_file</Name>"
                //         "            <Properties>"
                //         "                <Last-Modified>Mon, 12 Oct 2009 17:50:30 GMT</Last-Modified>"
                //         "                <Content-Length>787</Content-Length>"
                //         "            </Properties>"
                //         "        </Blob>"
                //         "        <BlobPrefix>"
                //         "           <Name>path/to/test_path/</Name>"
                //         "       </BlobPrefix>"
                //         "    </Blobs>"
                //         "    <NextMarker/>"
                //         "</EnumerationResults>");
                //
                // HarnessStorageInfoListCallbackData callbackData =
                // {
                //     .content = strNew(""),
                // };
                //
                // TEST_ERROR(
                //     storageInfoListP(storage, strNew("/"), hrnStorageInfoListCallback, NULL, .errorOnMissing = true),
                //     AssertError, "assertion '!param.errorOnMissing || storageFeature(this, storageFeaturePath)' failed");
                //
                // TEST_RESULT_VOID(
                //     storageInfoListP(storage, strNew("/path/to"), hrnStorageInfoListCallback, &callbackData), "list");
                // TEST_RESULT_STR_Z(
                //     callbackData.content,
                //     "test_path {path}\n"
                //     "test_file {file, s=787, t=1255369830}\n",
                //     "check");
                //
                // // -----------------------------------------------------------------------------------------------------------------
                // TEST_TITLE("list exists level");
                //
                // testRequestP(service, HTTP_VERB_GET, "?comp=list&delimiter=%2F&restype=container");
                // testResponseP(
                //     service,
                //     .content =
                //         "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
                //         "<EnumerationResults>"
                //         "    <Blobs>"
                //         "        <Blob>"
                //         "            <Name>test1.txt</Name>"
                //         "            <Properties/>"
                //         "        </Blob>"
                //         "        <BlobPrefix>"
                //         "            <Name>path1/</Name>"
                //         "        </BlobPrefix>"
                //         "    </Blobs>"
                //         "    <NextMarker/>"
                //         "</EnumerationResults>");
                //
                // callbackData.content = strNew("");
                //
                // TEST_RESULT_VOID(
                //     storageInfoListP(
                //         storage, strNew("/"), hrnStorageInfoListCallback, &callbackData, .level = storageInfoLevelExists),
                //     "list");
                // TEST_RESULT_STR_Z(
                //     callbackData.content,
                //     "path1 {}\n"
                //     "test1.txt {}\n",
                //     "check");
                //
                // // -----------------------------------------------------------------------------------------------------------------
                // TEST_TITLE("list a file in root with expression");
                //
                // testRequestP(service, HTTP_VERB_GET, "?comp=list&delimiter=%2F&prefix=test&restype=container");
                // testResponseP(
                //     service,
                //     .content =
                //         "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
                //         "<EnumerationResults>"
                //         "    <Blobs>"
                //         "        <Blob>"
                //         "            <Name>test1.txt</Name>"
                //         "            <Properties/>"
                //         "        </Blob>"
                //         "    </Blobs>"
                //         "    <NextMarker/>"
                //         "</EnumerationResults>");
                //
                // callbackData.content = strNew("");
                //
                // TEST_RESULT_VOID(
                //     storageInfoListP(
                //         storage, strNew("/"), hrnStorageInfoListCallback, &callbackData, .expression = strNew("^test.*$"),
                //         .level = storageInfoLevelExists),
                //     "list");
                // TEST_RESULT_STR_Z(
                //     callbackData.content,
                //     "test1.txt {}\n",
                //     "check");
                //
                // // -----------------------------------------------------------------------------------------------------------------
                // TEST_TITLE("list files with continuation");
                //
                // testRequestP(service, HTTP_VERB_GET, "?comp=list&delimiter=%2F&prefix=path%2Fto%2F&restype=container");
                // testResponseP(
                //     service,
                //     .content =
                //         "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
                //         "<EnumerationResults>"
                //         "    <Blobs>"
                //         "        <Blob>"
                //         "            <Name>path/to/test1.txt</Name>"
                //         "            <Properties/>"
                //         "        </Blob>"
                //         "        <Blob>"
                //         "            <Name>path/to/test2.txt</Name>"
                //         "            <Properties/>"
                //         "        </Blob>"
                //         "        <BlobPrefix>"
                //         "            <Name>path/to/path1/</Name>"
                //         "        </BlobPrefix>"
                //         "    </Blobs>"
                //         "    <NextMarker>ueGcxLPRx1Tr</NextMarker>"
                //         "</EnumerationResults>");
                //
                // testRequestP(
                //     service, HTTP_VERB_GET, "?comp=list&delimiter=%2F&marker=ueGcxLPRx1Tr&prefix=path%2Fto%2F&restype=container");
                // testResponseP(
                //     service,
                //     .content =
                //         "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
                //         "<EnumerationResults>"
                //         "    <Blobs>"
                //         "        <Blob>"
                //         "            <Name>path/to/test3.txt</Name>"
                //         "            <Properties/>"
                //         "        </Blob>"
                //         "        <BlobPrefix>"
                //         "            <Name>path/to/path2/</Name>"
                //         "        </BlobPrefix>"
                //         "    </Blobs>"
                //         "    <NextMarker/>"
                //         "</EnumerationResults>");
                //
                // callbackData.content = strNew("");
                //
                // TEST_RESULT_VOID(
                //     storageInfoListP(
                //         storage, strNew("/path/to"), hrnStorageInfoListCallback, &callbackData, .level = storageInfoLevelExists),
                //     "list");
                // TEST_RESULT_STR_Z(
                //     callbackData.content,
                //     "path1 {}\n"
                //     "test1.txt {}\n"
                //     "test2.txt {}\n"
                //     "path2 {}\n"
                //     "test3.txt {}\n",
                //     "check");
                //
                // // -----------------------------------------------------------------------------------------------------------------
                // TEST_TITLE("list files with expression");
                //
                // testRequestP(service, HTTP_VERB_GET, "?comp=list&delimiter=%2F&prefix=path%2Fto%2Ftest&restype=container");
                // testResponseP(
                //     service,
                //     .content =
                //         "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
                //         "<EnumerationResults>"
                //         "    <Blobs>"
                //         "        <Blob>"
                //         "            <Name>path/to/test1.txt</Name>"
                //         "            <Properties/>"
                //         "        </Blob>"
                //         "        <Blob>"
                //         "            <Name>path/to/test2.txt</Name>"
                //         "            <Properties/>"
                //         "        </Blob>"
                //         "        <Blob>"
                //         "            <Name>path/to/test3.txt</Name>"
                //         "            <Properties/>"
                //         "        </Blob>"
                //         "        <BlobPrefix>"
                //         "            <Name>path/to/test1.path/</Name>"
                //         "        </BlobPrefix>"
                //         "        <BlobPrefix>"
                //         "            <Name>path/to/test2.path/</Name>"
                //         "        </BlobPrefix>"
                //         "    </Blobs>"
                //         "    <NextMarker/>"
                //         "</EnumerationResults>");
                //
                // callbackData.content = strNew("");
                //
                // TEST_RESULT_VOID(
                //     storageInfoListP(
                //         storage, strNew("/path/to"), hrnStorageInfoListCallback, &callbackData, .expression = strNew("^test(1|3)"),
                //         .level = storageInfoLevelExists),
                //     "list");
                // TEST_RESULT_STR_Z(
                //     callbackData.content,
                //     "test1.path {}\n"
                //     "test1.txt {}\n"
                //     "test3.txt {}\n",
                //     "check");
                //
                // // -----------------------------------------------------------------------------------------------------------------
                // TEST_TITLE("switch to SAS auth");
                //
                // hrnServerScriptClose(service);
                //
                // hrnCfgArgRawZ(argList, cfgOptRepoGcsKeyType, STORAGE_GCS_KEY_TYPE_SAS);
                // hrnCfgEnvRawZ(cfgOptRepoGcsKey, TEST_KEY_SAS);
                // harnessCfgLoad(cfgCmdArchivePush, argList);
                //
                // TEST_ASSIGN(storage, storageRepoGet(0, true), "get repo storage");
                //
                // driver = (StorageGcs *)storage->driver;
                // TEST_RESULT_PTR_NE(driver->sasKey, NULL, "check sas key");
                //
                // hrnServerScriptAccept(service);
                //
                // // -----------------------------------------------------------------------------------------------------------------
                // TEST_TITLE("remove file");
                //
                // testRequestP(service, HTTP_VERB_DELETE, "/path/to/test.txt");
                // testResponseP(service);
                //
                // TEST_RESULT_VOID(storageRemoveP(storage, strNew("/path/to/test.txt")), "remove");
                //
                // // -----------------------------------------------------------------------------------------------------------------
                // TEST_TITLE("remove missing file");
                //
                // testRequestP(service, HTTP_VERB_DELETE, "/path/to/missing.txt");
                // testResponseP(service, .code = 404);
                //
                // TEST_RESULT_VOID(storageRemoveP(storage, strNew("/path/to/missing.txt")), "remove");
                //
                // // -----------------------------------------------------------------------------------------------------------------
                // TEST_TITLE("remove files error to check redacted sig");
                //
                // testRequestP(service, HTTP_VERB_GET, "?comp=list&restype=container");
                // testResponseP(service, .code = 403);
                //
                // TEST_ERROR_FMT(
                //     storagePathRemoveP(storage, strNew("/"), .recurse = true), ProtocolError,
                //     "HTTP request failed with 403 (Forbidden):\n"
                //     "*** Path/Query ***:\n"
                //     "/account/container?comp=list&restype=container&sig=<redacted>\n"
                //     "*** Request Headers ***:\n"
                //     "content-length: 0\n"
                //     "host: %s",
                //     strZ(hrnServerHost()));
                //
                // // -----------------------------------------------------------------------------------------------------------------
                // TEST_TITLE("remove files from root");
                //
                // testRequestP(service, HTTP_VERB_GET, "?comp=list&restype=container");
                // testResponseP(
                //     service,
                //     .content =
                //         "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
                //         "<EnumerationResults>"
                //         "    <Blobs>"
                //         "        <Blob>"
                //         "            <Name>test1.txt</Name>"
                //         "            <Properties/>"
                //         "        </Blob>"
                //         "        <Blob>"
                //         "            <Name>path1/xxx.zzz</Name>"
                //         "            <Properties/>"
                //         "        </Blob>"
                //         "        <BlobPrefix>"
                //         "            <Name>not-deleted/</Name>"
                //         "        </BlobPrefix>"
                //         "    </Blobs>"
                //         "    <NextMarker/>"
                //         "</EnumerationResults>");
                //
                // testRequestP(service, HTTP_VERB_DELETE, "/test1.txt");
                // testResponseP(service);
                //
                // testRequestP(service, HTTP_VERB_DELETE, "/path1/xxx.zzz");
                // testResponseP(service);
                //
                // TEST_RESULT_VOID(storagePathRemoveP(storage, strNew("/"), .recurse = true), "remove");
                //
                // // -----------------------------------------------------------------------------------------------------------------
                // TEST_TITLE("remove files from path");
                //
                // testRequestP(service, HTTP_VERB_GET, "?comp=list&prefix=path%2F&restype=container");
                // testResponseP(
                //     service,
                //     .content =
                //         "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
                //         "<EnumerationResults>"
                //         "    <Blobs>"
                //         "        <Blob>"
                //         "            <Name>path/test1.txt</Name>"
                //         "            <Properties/>"
                //         "        </Blob>"
                //         "        <Blob>"
                //         "            <Name>path/path1/xxx.zzz</Name>"
                //         "            <Properties/>"
                //         "        </Blob>"
                //         "        <BlobPrefix>"
                //         "            <Name>path/not-deleted/</Name>"
                //         "        </BlobPrefix>"
                //         "    </Blobs>"
                //         "    <NextMarker/>"
                //         "</EnumerationResults>");
                //
                // testRequestP(service, HTTP_VERB_DELETE, "/path/test1.txt");
                // testResponseP(service);
                //
                // testRequestP(service, HTTP_VERB_DELETE, "/path/path1/xxx.zzz");
                // testResponseP(service);
                //
                // TEST_RESULT_VOID(storagePathRemoveP(storage, strNew("/path"), .recurse = true), "remove");
                //
                // // -----------------------------------------------------------------------------------------------------------------
                // TEST_TITLE("remove files in empty subpath (nothing to do)");
                //
                // testRequestP(service, HTTP_VERB_GET, "?comp=list&prefix=path%2F&restype=container");
                // testResponseP(
                //     service,
                //     .content =
                //         "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
                //         "<EnumerationResults>"
                //         "    <Blobs>"
                //         "    </Blobs>"
                //         "    <NextMarker/>"
                //         "</EnumerationResults>");
                //
                // TEST_RESULT_VOID(storagePathRemoveP(storage, strNew("/path"), .recurse = true), "remove");

                // Auth service no longer needed
                hrnServerScriptEnd(auth);

                // -----------------------------------------------------------------------------------------------------------------
                hrnServerScriptEnd(service);
            }
            HARNESS_FORK_PARENT_END();
        }
        HARNESS_FORK_END();
    }

    FUNCTION_HARNESS_RESULT_VOID();
}
