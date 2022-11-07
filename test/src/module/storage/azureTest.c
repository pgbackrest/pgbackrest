/***********************************************************************************************************************************
Test Azure Storage
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
#define TEST_ACCOUNT                                                "account"
    STRING_STATIC(TEST_ACCOUNT_STR,                                 TEST_ACCOUNT);
#define TEST_CONTAINER                                              "container"
    STRING_STATIC(TEST_CONTAINER_STR,                               TEST_CONTAINER);
#define TEST_KEY_SAS                                                "?sig=key"
    STRING_STATIC(TEST_KEY_SAS_STR,                                 TEST_KEY_SAS);
#define TEST_KEY_SHARED                                             "YXpLZXk="
    STRING_STATIC(TEST_KEY_SHARED_STR,                              TEST_KEY_SHARED);

/***********************************************************************************************************************************
Helper to build test requests
***********************************************************************************************************************************/
static StorageAzure *driver;

typedef struct TestRequestParam
{
    VAR_PARAM_HEADER;
    const char *content;
    const char *blobType;
    const char *range;
} TestRequestParam;

#define testRequestP(write, verb, path, ...)                                                                                       \
    testRequest(write, verb, path, (TestRequestParam){VAR_PARAM_INIT, __VA_ARGS__})

static void
testRequest(IoWrite *write, const char *verb, const char *path, TestRequestParam param)
{
    String *request = strCatFmt(strNew(), "%s /" TEST_ACCOUNT "/" TEST_CONTAINER, verb);

    // When SAS spit out the query and merge in the SAS key
    if (driver->sasKey != NULL)
    {
        HttpQuery *query = httpQueryNewP();
        StringList *pathQuery = strLstNewSplitZ(STR(path), "?");

        if (strLstSize(pathQuery) == 2)
            query = httpQueryNewStr(strLstGet(pathQuery, 1));

        httpQueryMerge(query, driver->sasKey);

        strCat(request, strLstGet(pathQuery, 0));
        strCatZ(request, "?");
        strCat(request, httpQueryRenderP(query));
    }
    // Else just output path as is
    else
        strCatZ(request, path);

    // Add HTTP version and user agent
    strCatZ(request, " HTTP/1.1\r\nuser-agent:" PROJECT_NAME "/" PROJECT_VERSION "\r\n");

    // Add authorization string
    if (driver->sharedKey != NULL)
        strCatZ(request, "authorization:SharedKey account:????????????????????????????????????????????\r\n");

    // Add content-length
    strCatFmt(request, "content-length:%zu\r\n", param.content == NULL ? 0 : strlen(param.content));

    // Add md5
    if (param.content != NULL)
    {
        strCatFmt(
            request, "content-md5:%s\r\n", strZ(strNewEncode(encodeBase64, cryptoHashOne(hashTypeMd5, BUFSTRZ(param.content)))));
    }

    // Add date
    if (driver->sharedKey != NULL)
        strCatZ(request, "date:???, ?? ??? ???? ??:??:?? GMT\r\n");

    // Add host
    strCatFmt(request, "host:%s\r\n", strZ(hrnServerHost()));

    // Add range
    if (param.range != NULL)
        strCatFmt(request, "range:bytes=%s\r\n", param.range);

    // Add blob type
    if (param.blobType != NULL)
        strCatFmt(request, "x-ms-blob-type:%s\r\n", param.blobType);

    // Add version
    if (driver->sharedKey != NULL)
        strCatZ(request, "x-ms-version:2019-02-02\r\n");

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
    static const StorageHelper storageHelperList[] = {STORAGE_AZURE_HELPER, STORAGE_END_HELPER};
    storageHelperInit(storageHelperList);

    // *****************************************************************************************************************************
    if (testBegin("storageRepoGet()"))
    {
        // Test without the host option since that can't be run in a unit test without updating dns or /etc/hosts
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("storage with default options");

        StringList *argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test");
        hrnCfgArgRawStrId(argList, cfgOptRepoType, STORAGE_AZURE_TYPE);
        hrnCfgArgRawZ(argList, cfgOptRepoPath, "/repo");
        hrnCfgArgRawZ(argList, cfgOptRepoAzureContainer, TEST_CONTAINER);
        hrnCfgEnvRawZ(cfgOptRepoAzureAccount, TEST_ACCOUNT);
        hrnCfgEnvRawZ(cfgOptRepoAzureKey, TEST_KEY_SHARED);
        HRN_CFG_LOAD(cfgCmdArchivePush, argList);

        Storage *storage = NULL;
        TEST_ASSIGN(storage, storageRepoGet(0, false), "get repo storage");
        TEST_RESULT_STR_Z(storage->path, "/repo", "check path");
        TEST_RESULT_STR(((StorageAzure *)storageDriver(storage))->account, TEST_ACCOUNT_STR, "check account");
        TEST_RESULT_STR(((StorageAzure *)storageDriver(storage))->container, TEST_CONTAINER_STR, "check container");
        TEST_RESULT_STR(
            strNewEncode(encodeBase64, ((StorageAzure *)storageDriver(storage))->sharedKey), TEST_KEY_SHARED_STR, "check key");
        TEST_RESULT_STR_Z(((StorageAzure *)storageDriver(storage))->host, TEST_ACCOUNT ".blob.core.windows.net", "check host");
        TEST_RESULT_STR_Z(((StorageAzure *)storageDriver(storage))->pathPrefix, "/" TEST_CONTAINER, "check path prefix");
        TEST_RESULT_UINT(((StorageAzure *)storageDriver(storage))->blockSize, 4 * 1024 * 1024, "check block size");
        TEST_RESULT_BOOL(storageFeature(storage, storageFeaturePath), false, "check path feature");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("storage with host but force host-style uri");

        hrnCfgArgRawZ(argList, cfgOptRepoStorageHost, "https://test-host");
        hrnCfgArgRawStrId(argList, cfgOptRepoAzureUriStyle, storageAzureUriStyleHost);
        HRN_CFG_LOAD(cfgCmdArchivePush, argList);

        TEST_ASSIGN(storage, storageRepoGet(0, false), "get repo storage");
        TEST_RESULT_STR_Z(((StorageAzure *)storageDriver(storage))->host, TEST_ACCOUNT ".test-host", "check host");
        TEST_RESULT_STR_Z(((StorageAzure *)storageDriver(storage))->pathPrefix, "/" TEST_CONTAINER, "check path prefix");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("storage with https protocol, appended port, uristylepath");

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test");
        hrnCfgArgRawStrId(argList, cfgOptRepoType, STORAGE_AZURE_TYPE);
        hrnCfgArgRawZ(argList, cfgOptRepoPath, "/repo");
        hrnCfgArgRawZ(argList, cfgOptRepoAzureContainer, TEST_CONTAINER);
        hrnCfgEnvRawZ(cfgOptRepoAzureAccount, TEST_ACCOUNT);
        hrnCfgEnvRawZ(cfgOptRepoAzureKey, TEST_KEY_SHARED);
        HRN_CFG_LOAD(cfgCmdArchivePush, argList);

        hrnCfgArgRawZ(argList, cfgOptRepoStorageHost, "https://test-host:443");
        hrnCfgArgRawStrId(argList, cfgOptRepoAzureUriStyle, storageAzureUriStylePath);
        HRN_CFG_LOAD(cfgCmdArchivePush, argList);

        TEST_ASSIGN(storage, storageRepoGet(0, false), "get repo storage");
        TEST_RESULT_STR_Z(((StorageAzure *)storageDriver(storage))->host, "test-host", "check host");
        TEST_RESULT_STR_Z(
            ((StorageAzure *)storageDriver(storage))->pathPrefix, "/" TEST_ACCOUNT "/" TEST_CONTAINER, "check path prefix");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("storage with no protocol, appended port, uristylepath");

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test");
        hrnCfgArgRawStrId(argList, cfgOptRepoType, STORAGE_AZURE_TYPE);
        hrnCfgArgRawZ(argList, cfgOptRepoPath, "/repo");
        hrnCfgArgRawZ(argList, cfgOptRepoAzureContainer, TEST_CONTAINER);
        hrnCfgEnvRawZ(cfgOptRepoAzureAccount, TEST_ACCOUNT);
        hrnCfgEnvRawZ(cfgOptRepoAzureKey, TEST_KEY_SHARED);
        HRN_CFG_LOAD(cfgCmdArchivePush, argList);

        hrnCfgArgRawZ(argList, cfgOptRepoStorageHost, "test-host:443");
        hrnCfgArgRawStrId(argList, cfgOptRepoAzureUriStyle, storageAzureUriStylePath);
        HRN_CFG_LOAD(cfgCmdArchivePush, argList);

        TEST_ASSIGN(storage, storageRepoGet(0, false), "get repo storage");
        TEST_RESULT_STR_Z(((StorageAzure *)storageDriver(storage))->host, "test-host", "check host");
        TEST_RESULT_STR_Z(
            ((StorageAzure *)storageDriver(storage))->pathPrefix, "/" TEST_ACCOUNT "/" TEST_CONTAINER, "check path prefix");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("storage with https protocol, appended port, uristylehost");

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test");
        hrnCfgArgRawStrId(argList, cfgOptRepoType, STORAGE_AZURE_TYPE);
        hrnCfgArgRawZ(argList, cfgOptRepoPath, "/repo");
        hrnCfgArgRawZ(argList, cfgOptRepoAzureContainer, TEST_CONTAINER);
        hrnCfgEnvRawZ(cfgOptRepoAzureAccount, TEST_ACCOUNT);
        hrnCfgEnvRawZ(cfgOptRepoAzureKey, TEST_KEY_SHARED);
        HRN_CFG_LOAD(cfgCmdArchivePush, argList);

        hrnCfgArgRawZ(argList, cfgOptRepoStorageHost, "https://test-host:443");
        hrnCfgArgRawStrId(argList, cfgOptRepoAzureUriStyle, storageAzureUriStyleHost);
        HRN_CFG_LOAD(cfgCmdArchivePush, argList);

        TEST_ASSIGN(storage, storageRepoGet(0, false), "get repo storage");
        TEST_RESULT_STR_Z(((StorageAzure *)storageDriver(storage))->host, TEST_ACCOUNT ".test-host", "check host");
        TEST_RESULT_STR_Z(((StorageAzure *)storageDriver(storage))->pathPrefix, "/" TEST_CONTAINER, "check path prefix");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("storage with no protocol, appended port, uristylehost");

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test");
        hrnCfgArgRawStrId(argList, cfgOptRepoType, STORAGE_AZURE_TYPE);
        hrnCfgArgRawZ(argList, cfgOptRepoPath, "/repo");
        hrnCfgArgRawZ(argList, cfgOptRepoAzureContainer, TEST_CONTAINER);
        hrnCfgEnvRawZ(cfgOptRepoAzureAccount, TEST_ACCOUNT);
        hrnCfgEnvRawZ(cfgOptRepoAzureKey, TEST_KEY_SHARED);
        HRN_CFG_LOAD(cfgCmdArchivePush, argList);

        hrnCfgArgRawZ(argList, cfgOptRepoStorageHost, "test-host:443");
        hrnCfgArgRawStrId(argList, cfgOptRepoAzureUriStyle, storageAzureUriStyleHost);
        HRN_CFG_LOAD(cfgCmdArchivePush, argList);

        TEST_ASSIGN(storage, storageRepoGet(0, false), "get repo storage");
        TEST_RESULT_STR_Z(((StorageAzure *)storageDriver(storage))->host, TEST_ACCOUNT ".test-host", "check host");
        TEST_RESULT_STR_Z(((StorageAzure *)storageDriver(storage))->pathPrefix, "/" TEST_CONTAINER, "check path prefix");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("storage with no protocol, with appended port, default uristyle ");

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test");
        hrnCfgArgRawStrId(argList, cfgOptRepoType, STORAGE_AZURE_TYPE);
        hrnCfgArgRawZ(argList, cfgOptRepoPath, "/repo");
        hrnCfgArgRawZ(argList, cfgOptRepoAzureContainer, TEST_CONTAINER);
        hrnCfgEnvRawZ(cfgOptRepoAzureAccount, TEST_ACCOUNT);
        hrnCfgEnvRawZ(cfgOptRepoAzureKey, TEST_KEY_SHARED);
        HRN_CFG_LOAD(cfgCmdArchivePush, argList);

        hrnCfgArgRawZ(argList, cfgOptRepoStorageHost, "test-host:443");
        HRN_CFG_LOAD(cfgCmdArchivePush, argList);

        TEST_ASSIGN(storage, storageRepoGet(0, false), "get repo storage");
        TEST_RESULT_STR_Z(((StorageAzure *)storageDriver(storage))->host, "test-host", "check host");
        TEST_RESULT_STR_Z(
            ((StorageAzure *)storageDriver(storage))->pathPrefix, "/" TEST_ACCOUNT "/" TEST_CONTAINER, "check path prefix");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("storage with no protocol, specified port, uristylehost");

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test");
        hrnCfgArgRawStrId(argList, cfgOptRepoType, STORAGE_AZURE_TYPE);
        hrnCfgArgRawZ(argList, cfgOptRepoPath, "/repo");
        hrnCfgArgRawZ(argList, cfgOptRepoAzureContainer, TEST_CONTAINER);
        hrnCfgEnvRawZ(cfgOptRepoAzureAccount, TEST_ACCOUNT);
        hrnCfgEnvRawZ(cfgOptRepoAzureKey, TEST_KEY_SHARED);
        HRN_CFG_LOAD(cfgCmdArchivePush, argList);

        hrnCfgArgRawZ(argList, cfgOptRepoStorageHost, "test-host");
        hrnCfgArgRawFmt(argList, cfgOptRepoStoragePort, "%u", (const unsigned int) 443);
        hrnCfgArgRawStrId(argList, cfgOptRepoAzureUriStyle, storageAzureUriStyleHost);
        HRN_CFG_LOAD(cfgCmdArchivePush, argList);

        TEST_ASSIGN(storage, storageRepoGet(0, false), "get repo storage");
        TEST_RESULT_STR_Z(((StorageAzure *)storageDriver(storage))->host, TEST_ACCOUNT ".test-host", "check host");
        TEST_RESULT_STR_Z(((StorageAzure *)storageDriver(storage))->pathPrefix, "/" TEST_CONTAINER, "check path prefix");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("storage with no protocol, appended port, specified port");

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test");
        hrnCfgArgRawStrId(argList, cfgOptRepoType, STORAGE_AZURE_TYPE);
        hrnCfgArgRawZ(argList, cfgOptRepoPath, "/repo");
        hrnCfgArgRawZ(argList, cfgOptRepoAzureContainer, TEST_CONTAINER);
        hrnCfgEnvRawZ(cfgOptRepoAzureAccount, TEST_ACCOUNT);
        hrnCfgEnvRawZ(cfgOptRepoAzureKey, TEST_KEY_SHARED);
        HRN_CFG_LOAD(cfgCmdArchivePush, argList);

        hrnCfgArgRawZ(argList, cfgOptRepoStorageHost, "test-host:8443");
        hrnCfgArgRawFmt(argList, cfgOptRepoStoragePort, "%u", (const unsigned int) 8443);
        HRN_CFG_LOAD(cfgCmdArchivePush, argList);

        TEST_ASSIGN(storage, storageRepoGet(0, false), "get repo storage");
        TEST_RESULT_STR_Z(((StorageAzure *)storageDriver(storage))->host, "test-host", "check host");
        TEST_RESULT_STR_Z(
            ((StorageAzure *)storageDriver(storage))->pathPrefix, "/" TEST_ACCOUNT "/" TEST_CONTAINER, "check path prefix");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("invalid shared key base64");

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test");
        hrnCfgArgRawStrId(argList, cfgOptRepoType, STORAGE_AZURE_TYPE);
        hrnCfgArgRawZ(argList, cfgOptRepoAzureContainer, TEST_CONTAINER);
        hrnCfgEnvRawZ(cfgOptRepoAzureAccount, TEST_ACCOUNT);
        hrnCfgEnvRawZ(cfgOptRepoAzureKey, BOGUS_STR);

        HRN_CFG_LOAD(cfgCmdArchivePush, argList);

        TEST_ERROR(
            storageRepoGet(0, false), OptionInvalidValueError,
            "invalid value for 'repo1-azure-key' option: base64 size 5 is not evenly divisible by 4\n"
            "HINT: value must be valid base64 when 'repo1-azure-key-type = shared'.");
    }

    // *****************************************************************************************************************************
    if (testBegin("storageAzureAuth()"))
    {
        StorageAzure *storage = NULL;
        HttpHeader *header = NULL;
        const String *dateTime = STRDEF("Sun, 21 Jun 2020 12:46:19 GMT");

        TEST_ASSIGN(
            storage,
            (StorageAzure *)storageDriver(
                storageAzureNew(
                    STRDEF("/repo"), false, NULL, TEST_CONTAINER_STR, TEST_ACCOUNT_STR, storageAzureKeyTypeShared,
                    TEST_KEY_SHARED_STR, 16, STRDEF("blob.core.windows.net"), storageAzureUriStyleHost, 443, 1000, true, NULL,
                    NULL)),
            "new azure storage - shared key");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("minimal auth");

        header = httpHeaderAdd(httpHeaderNew(NULL), HTTP_HEADER_CONTENT_LENGTH_STR, ZERO_STR);

        TEST_RESULT_VOID(storageAzureAuth(storage, HTTP_VERB_GET_STR, STRDEF("/path"), NULL, dateTime, header), "auth");
        TEST_RESULT_STR_Z(
            httpHeaderToLog(header),
            "{authorization: 'SharedKey account:edqgT7EhsiIN3q6Al2HCZlpXr2D5cJFavr2ZCkhG9R8=', content-length: '0'"
                ", date: 'Sun, 21 Jun 2020 12:46:19 GMT', host: 'account.blob.core.windows.net', x-ms-version: '2019-02-02'}",
            "check headers");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("auth with md5 and query");

        header = httpHeaderAdd(httpHeaderNew(NULL), HTTP_HEADER_CONTENT_LENGTH_STR, STRDEF("44"));
        httpHeaderAdd(header, HTTP_HEADER_CONTENT_MD5_STR, STRDEF("b64f49553d5c441652e95697a2c5949e"));

        HttpQuery *query = httpQueryAdd(httpQueryNewP(), STRDEF("a"), STRDEF("b"));

        TEST_RESULT_VOID(storageAzureAuth(storage, HTTP_VERB_GET_STR, STRDEF("/path/file"), query, dateTime, header), "auth");
        TEST_RESULT_STR_Z(
            httpHeaderToLog(header),
            "{authorization: 'SharedKey account:5qAnroLtbY8IWqObx8+UVwIUysXujsfWZZav7PrBON0=', content-length: '44'"
                ", content-md5: 'b64f49553d5c441652e95697a2c5949e', date: 'Sun, 21 Jun 2020 12:46:19 GMT'"
                ", host: 'account.blob.core.windows.net', x-ms-version: '2019-02-02'}",
            "check headers");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("SAS auth");

        TEST_ASSIGN(
            storage,
            (StorageAzure *)storageDriver(
                storageAzureNew(
                    STRDEF("/repo"), false, NULL, TEST_CONTAINER_STR, TEST_ACCOUNT_STR, storageAzureKeyTypeSas, TEST_KEY_SAS_STR,
                    16, STRDEF("blob.core.usgovcloudapi.net"), storageAzureUriStyleHost, 443, 1000, true, NULL, NULL)),
            "new azure storage - sas key");

        query = httpQueryAdd(httpQueryNewP(), STRDEF("a"), STRDEF("b"));
        header = httpHeaderAdd(httpHeaderNew(NULL), HTTP_HEADER_CONTENT_LENGTH_STR, STRDEF("66"));

        TEST_RESULT_VOID(storageAzureAuth(storage, HTTP_VERB_GET_STR, STRDEF("/path/file"), query, dateTime, header), "auth");
        TEST_RESULT_STR_Z(
            httpHeaderToLog(header), "{content-length: '66', host: 'account.blob.core.usgovcloudapi.net'}", "check headers");
        TEST_RESULT_STR_Z(httpQueryRenderP(query), "a=b&sig=key", "check query");
    }

    // *****************************************************************************************************************************
    if (testBegin("StorageAzure, StorageReadAzure, and StorageWriteAzure"))
    {
        HRN_FORK_BEGIN()
        {
            HRN_FORK_CHILD_BEGIN(.prefix = "azure server", .timeout = 5000)
            {
                TEST_RESULT_VOID(hrnServerRunP(HRN_FORK_CHILD_READ(), hrnServerProtocolTls), "azure server run");
            }
            HRN_FORK_CHILD_END();

            HRN_FORK_PARENT_BEGIN(.prefix = "azure client")
            {
                IoWrite *service = hrnServerScriptBegin(HRN_FORK_PARENT_WRITE(0));

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("test against local host with path-style URIs");

                StringList *argList = strLstNew();
                hrnCfgArgRawZ(argList, cfgOptStanza, "test");
                hrnCfgArgRawStrId(argList, cfgOptRepoType, STORAGE_AZURE_TYPE);
                hrnCfgArgRawZ(argList, cfgOptRepoPath, "/");
                hrnCfgArgRawZ(argList, cfgOptRepoAzureContainer, TEST_CONTAINER);
                hrnCfgArgRawFmt(argList, cfgOptRepoStorageHost, "https://%s:%u", strZ(hrnServerHost()), hrnServerPort(0));
                hrnCfgArgRawBool(argList, cfgOptRepoStorageVerifyTls, TEST_IN_CONTAINER);
                hrnCfgEnvRawZ(cfgOptRepoAzureAccount, TEST_ACCOUNT);
                hrnCfgEnvRawZ(cfgOptRepoAzureKey, TEST_KEY_SHARED);
                HRN_CFG_LOAD(cfgCmdArchivePush, argList);

                Storage *storage = NULL;
                TEST_ASSIGN(storage, storageRepoGet(0, true), "get repo storage");

                driver = (StorageAzure *)storageDriver(storage);
                TEST_RESULT_STR(driver->host, hrnServerHost(), "check host");
                TEST_RESULT_STR_Z(driver->pathPrefix, "/" TEST_ACCOUNT "/" TEST_CONTAINER, "check path prefix");
                TEST_RESULT_BOOL(driver->fileId == 0, false, "check file id");

                // Tests need the block size to be 16
                driver->blockSize = 16;

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("ignore missing file");

                hrnServerScriptAccept(service);
                testRequestP(service, HTTP_VERB_GET, "/fi%26le.txt");
                testResponseP(service, .code = 404);

                TEST_RESULT_PTR(
                    storageGetP(storageNewReadP(storage, STRDEF("fi&le.txt"), .ignoreMissing = true)), NULL, "get file");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("error on missing file");

                testRequestP(service, HTTP_VERB_GET, "/file.txt");
                testResponseP(service, .code = 404);

                TEST_ERROR(
                    storageGetP(storageNewReadP(storage, STRDEF("file.txt"))), FileMissingError,
                    "unable to open missing file '/file.txt' for read");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("get file with offset and limit");

                testRequestP(service, HTTP_VERB_GET, "/file.txt", .range = "1-21");
                testResponseP(service, .content = "this is a sample file");

                TEST_RESULT_STR_Z(
                    strNewBuf(storageGetP(storageNewReadP(storage, STRDEF("file.txt"), .offset = 1, .limit = VARUINT64(21)))),
                    "this is a sample file", "get file");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("get zero-length file");

                testRequestP(service, HTTP_VERB_GET, "/file0.txt");
                testResponseP(service);

                TEST_RESULT_STR_Z(
                    strNewBuf(storageGetP(storageNewReadP(storage, STRDEF("file0.txt")))), "", "get zero-length file");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("non-404 error");

                testRequestP(service, HTTP_VERB_GET, "/file.txt");
                testResponseP(service, .code = 303, .content = "CONTENT");

                StorageRead *read = NULL;
                TEST_ASSIGN(read, storageNewReadP(storage, STRDEF("file.txt"), .ignoreMissing = true), "new read file");
                TEST_RESULT_BOOL(storageReadIgnoreMissing(read), true, "check ignore missing");
                TEST_RESULT_STR_Z(storageReadName(read), "/file.txt", "check name");

                TEST_ERROR_FMT(
                    ioReadOpen(storageReadIo(read)), ProtocolError,
                    "HTTP request failed with 303:\n"
                    "*** Path/Query ***:\n"
                    "GET /account/container/file.txt\n"
                    "*** Request Headers ***:\n"
                    "authorization: <redacted>\n"
                    "content-length: 0\n"
                    "date: <redacted>\n"
                    "host: %s\n"
                    "x-ms-version: 2019-02-02\n"
                    "*** Response Headers ***:\n"
                    "content-length: 7\n"
                    "*** Response Content ***:\n"
                    "CONTENT",
                    strZ(hrnServerHost()));

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("write error");

                testRequestP(service, HTTP_VERB_PUT, "/file.txt", .blobType = "BlockBlob", .content = "ABCD");
                testResponseP(service, .code = 403);

                TEST_ERROR_FMT(
                    storagePutP(storageNewWriteP(storage, STRDEF("file.txt")), BUFSTRDEF("ABCD")), ProtocolError,
                    "HTTP request failed with 403 (Forbidden):\n"
                    "*** Path/Query ***:\n"
                    "PUT /account/container/file.txt\n"
                    "*** Request Headers ***:\n"
                    "authorization: <redacted>\n"
                    "content-length: 4\n"
                    "content-md5: ywjKSnu1+Wg8GRM6hIcspw==\n"
                    "date: <redacted>\n"
                    "host: %s\n"
                    "x-ms-blob-type: BlockBlob\n"
                    "x-ms-version: 2019-02-02",
                    strZ(hrnServerHost()));

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("write file in one part (with retry)");

                testRequestP(service, HTTP_VERB_PUT, "/file.txt", .blobType = "BlockBlob", .content = "ABCD");
                testResponseP(service, .code = 503);
                testRequestP(service, HTTP_VERB_PUT, "/file.txt", .blobType = "BlockBlob", .content = "ABCD");
                testResponseP(service);

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
                TEST_RESULT_BOOL(storageWriteTruncate(write), true, "file will be truncated");

                TEST_RESULT_VOID(storageWriteAzureClose(write->driver), "close file again");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("write zero-length file");

                testRequestP(service, HTTP_VERB_PUT, "/file.txt", .blobType = "BlockBlob", .content = "");
                testResponseP(service);

                TEST_ASSIGN(write, storageNewWriteP(storage, STRDEF("file.txt")), "new write");
                TEST_RESULT_VOID(storagePutP(write, NULL), "write");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("write file in chunks with nothing left over on close");

                testRequestP(
                    service, HTTP_VERB_PUT, "/file.txt?blockid=0AAAAAAACCCCCCCCx0000000&comp=block", .content = "1234567890123456");
                testResponseP(service);

                testRequestP(
                    service, HTTP_VERB_PUT, "/file.txt?blockid=0AAAAAAACCCCCCCCx0000001&comp=block", .content = "7890123456789012");
                testResponseP(service);

                testRequestP(
                    service, HTTP_VERB_PUT, "/file.txt?comp=blocklist",
                    .content =
                        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                        "<BlockList>"
                        "<Uncommitted>0AAAAAAACCCCCCCCx0000000</Uncommitted>"
                        "<Uncommitted>0AAAAAAACCCCCCCCx0000001</Uncommitted>"
                        "</BlockList>\n");
                testResponseP(service);

                // Test needs a predictable file id
                driver->fileId = 0x0AAAAAAACCCCCCCC;

                TEST_ASSIGN(write, storageNewWriteP(storage, STRDEF("file.txt")), "new write");
                TEST_RESULT_VOID(storagePutP(write, BUFSTRDEF("12345678901234567890123456789012")), "write");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("write file in chunks with something left over on close");

                testRequestP(
                    service, HTTP_VERB_PUT, "/file.txt?blockid=0AAAAAAACCCCCCCDx0000000&comp=block", .content = "1234567890123456");
                testResponseP(service);

                testRequestP(
                    service, HTTP_VERB_PUT, "/file.txt?blockid=0AAAAAAACCCCCCCDx0000001&comp=block", .content = "7890");
                testResponseP(service);

                testRequestP(
                    service, HTTP_VERB_PUT, "/file.txt?comp=blocklist",
                    .content =
                        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                        "<BlockList>"
                        "<Uncommitted>0AAAAAAACCCCCCCDx0000000</Uncommitted>"
                        "<Uncommitted>0AAAAAAACCCCCCCDx0000001</Uncommitted>"
                        "</BlockList>\n");
                testResponseP(service);

                TEST_ASSIGN(write, storageNewWriteP(storage, STRDEF("file.txt")), "new write");
                TEST_RESULT_VOID(storagePutP(write, BUFSTRDEF("12345678901234567890")), "write");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("info for / does not exist");

                TEST_RESULT_BOOL(storageInfoP(storage, NULL, .ignoreMissing = true).exists, false, "info for /");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("info for missing file");

                testRequestP(service, HTTP_VERB_HEAD, "/BOGUS");
                testResponseP(service, .code = 404);

                TEST_RESULT_BOOL(
                    storageInfoP(storage, STRDEF("BOGUS"), .ignoreMissing = true).exists, false, "file does not exist");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("info for file");

                testRequestP(service, HTTP_VERB_HEAD, "/subdir/file1.txt");
                testResponseP(service, .header = "content-length:9999\r\nLast-Modified: Wed, 21 Oct 2015 07:28:00 GMT");

                StorageInfo info;
                TEST_ASSIGN(info, storageInfoP(storage, STRDEF("subdir/file1.txt")), "file exists");
                TEST_RESULT_BOOL(info.exists, true, "check exists");
                TEST_RESULT_UINT(info.type, storageTypeFile, "check type");
                TEST_RESULT_UINT(info.size, 9999, "check exists");
                TEST_RESULT_INT(info.timeModified, 1445412480, "check time");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("info check existence only");

                testRequestP(service, HTTP_VERB_HEAD, "/subdir/file2.txt");
                testResponseP(service, .header = "content-length:777\r\nLast-Modified: Wed, 22 Oct 2015 07:28:00 GMT");

                TEST_ASSIGN(
                    info, storageInfoP(storage, STRDEF("subdir/file2.txt"), .level = storageInfoLevelExists), "file exists");
                TEST_RESULT_BOOL(info.exists, true, "check exists");
                TEST_RESULT_UINT(info.type, storageTypeFile, "check type");
                TEST_RESULT_UINT(info.size, 0, "check exists");
                TEST_RESULT_INT(info.timeModified, 0, "check time");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("list basic level");

                testRequestP(service, HTTP_VERB_GET, "?comp=list&delimiter=%2F&prefix=path%2Fto%2F&restype=container");
                testResponseP(
                    service,
                    .content =
                        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
                        "<EnumerationResults>"
                        "    <Blobs>"
                        "        <Blob>"
                        "            <Name>path/to/test_file</Name>"
                        "            <Properties>"
                        "                <Last-Modified>Mon, 12 Oct 2009 17:50:30 GMT</Last-Modified>"
                        "                <Content-Length>787</Content-Length>"
                        "            </Properties>"
                        "        </Blob>"
                        "        <BlobPrefix>"
                        "           <Name>path/to/test_path/</Name>"
                        "       </BlobPrefix>"
                        "    </Blobs>"
                        "    <NextMarker/>"
                        "</EnumerationResults>");

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

                testRequestP(service, HTTP_VERB_GET, "?comp=list&delimiter=%2F&restype=container");
                testResponseP(
                    service,
                    .content =
                        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
                        "<EnumerationResults>"
                        "    <Blobs>"
                        "        <Blob>"
                        "            <Name>test1.txt</Name>"
                        "            <Properties/>"
                        "        </Blob>"
                        "        <BlobPrefix>"
                        "            <Name>path1/</Name>"
                        "        </BlobPrefix>"
                        "    </Blobs>"
                        "    <NextMarker/>"
                        "</EnumerationResults>");

                TEST_STORAGE_LIST(
                    storage, "/",
                    "path1/\n"
                    "test1.txt\n",
                    .noRecurse = true);

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("list a file in root with expression");

                testRequestP(service, HTTP_VERB_GET, "?comp=list&delimiter=%2F&prefix=test&restype=container");
                testResponseP(
                    service,
                    .content =
                        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
                        "<EnumerationResults>"
                        "    <Blobs>"
                        "        <Blob>"
                        "            <Name>test1.txt</Name>"
                        "            <Properties/>"
                        "        </Blob>"
                        "    </Blobs>"
                        "    <NextMarker/>"
                        "</EnumerationResults>");

                TEST_STORAGE_LIST(
                    storage, "/",
                    "test1.txt\n",
                    .noRecurse = true, .expression = "^test.*$");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("list files with continuation");

                testRequestP(service, HTTP_VERB_GET, "?comp=list&delimiter=%2F&prefix=path%2Fto%2F&restype=container");
                testResponseP(
                    service,
                    .content =
                        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
                        "<EnumerationResults>"
                        "    <Blobs>"
                        "        <Blob>"
                        "            <Name>path/to/test1.txt</Name>"
                        "            <Properties/>"
                        "        </Blob>"
                        "        <Blob>"
                        "            <Name>path/to/test2.txt</Name>"
                        "            <Properties/>"
                        "        </Blob>"
                        "        <BlobPrefix>"
                        "            <Name>path/to/path1/</Name>"
                        "        </BlobPrefix>"
                        "    </Blobs>"
                        "    <NextMarker>ueGcxLPRx1Tr</NextMarker>"
                        "</EnumerationResults>");

                testRequestP(
                    service, HTTP_VERB_GET, "?comp=list&delimiter=%2F&marker=ueGcxLPRx1Tr&prefix=path%2Fto%2F&restype=container");
                testResponseP(
                    service,
                    .content =
                        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
                        "<EnumerationResults>"
                        "    <Blobs>"
                        "        <Blob>"
                        "            <Name>path/to/test3.txt</Name>"
                        "            <Properties/>"
                        "        </Blob>"
                        "        <BlobPrefix>"
                        "            <Name>path/to/path2/</Name>"
                        "        </BlobPrefix>"
                        "    </Blobs>"
                        "    <NextMarker/>"
                        "</EnumerationResults>");

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

                testRequestP(service, HTTP_VERB_GET, "?comp=list&delimiter=%2F&prefix=path%2Fto%2Ftest&restype=container");
                testResponseP(
                    service,
                    .content =
                        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
                        "<EnumerationResults>"
                        "    <Blobs>"
                        "        <Blob>"
                        "            <Name>path/to/test1.txt</Name>"
                        "            <Properties/>"
                        "        </Blob>"
                        "        <Blob>"
                        "            <Name>path/to/test2.txt</Name>"
                        "            <Properties/>"
                        "        </Blob>"
                        "        <Blob>"
                        "            <Name>path/to/test3.txt</Name>"
                        "            <Properties/>"
                        "        </Blob>"
                        "        <BlobPrefix>"
                        "            <Name>path/to/test1.path/</Name>"
                        "        </BlobPrefix>"
                        "        <BlobPrefix>"
                        "            <Name>path/to/test2.path/</Name>"
                        "        </BlobPrefix>"
                        "    </Blobs>"
                        "    <NextMarker/>"
                        "</EnumerationResults>");

                TEST_STORAGE_LIST(
                    storage, "/path/to",
                    "test1.path\n"
                    "test1.txt\n"
                    "test3.txt\n",
                    .level = storageInfoLevelExists, .noRecurse = true, .expression = "^test(1|3)");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("switch to SAS auth");

                hrnServerScriptClose(service);

                hrnCfgArgRawStrId(argList, cfgOptRepoAzureKeyType, storageAzureKeyTypeSas);
                hrnCfgEnvRawZ(cfgOptRepoAzureKey, TEST_KEY_SAS);
                HRN_CFG_LOAD(cfgCmdArchivePush, argList);

                TEST_ASSIGN(storage, storageRepoGet(0, true), "get repo storage");

                driver = (StorageAzure *)storageDriver(storage);
                TEST_RESULT_PTR_NE(driver->sasKey, NULL, "check sas key");

                hrnServerScriptAccept(service);

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("remove file");

                testRequestP(service, HTTP_VERB_DELETE, "/path/to/test.txt");
                testResponseP(service);

                TEST_RESULT_VOID(storageRemoveP(storage, STRDEF("/path/to/test.txt")), "remove");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("remove missing file");

                testRequestP(service, HTTP_VERB_DELETE, "/path/to/missing.txt");
                testResponseP(service, .code = 404);

                TEST_RESULT_VOID(storageRemoveP(storage, STRDEF("/path/to/missing.txt")), "remove");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("remove files error to check redacted sig");

                testRequestP(service, HTTP_VERB_GET, "?comp=list&restype=container");
                testResponseP(service, .code = 403);

                TEST_ERROR_FMT(
                    storagePathRemoveP(storage, STRDEF("/"), .recurse = true), ProtocolError,
                    "HTTP request failed with 403 (Forbidden):\n"
                    "*** Path/Query ***:\n"
                    "GET /account/container?comp=list&restype=container&sig=<redacted>\n"
                    "*** Request Headers ***:\n"
                    "content-length: 0\n"
                    "host: %s",
                    strZ(hrnServerHost()));

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("remove files from root");

                testRequestP(service, HTTP_VERB_GET, "?comp=list&restype=container");
                testResponseP(
                    service,
                    .content =
                        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
                        "<EnumerationResults>"
                        "    <Blobs>"
                        "        <Blob>"
                        "            <Name>test1.txt</Name>"
                        "            <Properties/>"
                        "        </Blob>"
                        "        <Blob>"
                        "            <Name>path1/xxx.zzz</Name>"
                        "            <Properties/>"
                        "        </Blob>"
                        "        <BlobPrefix>"
                        "            <Name>not-deleted/</Name>"
                        "        </BlobPrefix>"
                        "    </Blobs>"
                        "    <NextMarker/>"
                        "</EnumerationResults>");

                testRequestP(service, HTTP_VERB_DELETE, "/test1.txt");
                testResponseP(service);

                testRequestP(service, HTTP_VERB_DELETE, "/path1/xxx.zzz");
                testResponseP(service);

                TEST_RESULT_VOID(storagePathRemoveP(storage, STRDEF("/"), .recurse = true), "remove");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("remove files from path");

                testRequestP(service, HTTP_VERB_GET, "?comp=list&prefix=path%2F&restype=container");
                testResponseP(
                    service,
                    .content =
                        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
                        "<EnumerationResults>"
                        "    <Blobs>"
                        "        <Blob>"
                        "            <Name>path/test1.txt</Name>"
                        "            <Properties/>"
                        "        </Blob>"
                        "        <Blob>"
                        "            <Name>path/path1/xxx.zzz</Name>"
                        "            <Properties/>"
                        "        </Blob>"
                        "        <BlobPrefix>"
                        "            <Name>path/not-deleted/</Name>"
                        "        </BlobPrefix>"
                        "    </Blobs>"
                        "    <NextMarker/>"
                        "</EnumerationResults>");

                testRequestP(service, HTTP_VERB_DELETE, "/path/test1.txt");
                testResponseP(service);

                testRequestP(service, HTTP_VERB_DELETE, "/path/path1/xxx.zzz");
                testResponseP(service);

                TEST_RESULT_VOID(storagePathRemoveP(storage, STRDEF("/path"), .recurse = true), "remove");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("remove files in empty subpath (nothing to do)");

                testRequestP(service, HTTP_VERB_GET, "?comp=list&prefix=path%2F&restype=container");
                testResponseP(
                    service,
                    .content =
                        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
                        "<EnumerationResults>"
                        "    <Blobs>"
                        "    </Blobs>"
                        "    <NextMarker/>"
                        "</EnumerationResults>");

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
