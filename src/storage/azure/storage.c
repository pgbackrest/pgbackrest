/***********************************************************************************************************************************
Azure Storage
***********************************************************************************************************************************/
#include "build.auto.h"

#include <string.h>

#include "common/crypto/common.h"
#include "common/crypto/hash.h"
#include "common/debug.h"
#include "common/io/http/client.h"
#include "common/io/http/common.h"
#include "common/io/socket/client.h"
#include "common/io/tls/client.h"
#include "common/log.h"
#include "common/regExp.h"
#include "common/type/object.h"
#include "common/type/xml.h"
#include "storage/azure/read.h"
#include "storage/azure/storage.intern.h"
#include "storage/azure/write.h"

/***********************************************************************************************************************************
Azure http headers
***********************************************************************************************************************************/
STRING_STATIC(AZURE_HEADER_VERSION_STR,                             "x-ms-version");
STRING_STATIC(AZURE_HEADER_VERSION_VALUE_STR,                       "2019-02-02");

/***********************************************************************************************************************************
Azure query tokens
***********************************************************************************************************************************/
STRING_STATIC(AZURE_QUERY_MARKER_STR,                               "marker");
STRING_EXTERN(AZURE_QUERY_COMP_STR,                                 AZURE_QUERY_COMP);
STRING_STATIC(AZURE_QUERY_DELIMITER_STR,                            "delimiter");
STRING_STATIC(AZURE_QUERY_PREFIX_STR,                               "prefix");
STRING_EXTERN(AZURE_QUERY_RESTYPE_STR,                              AZURE_QUERY_RESTYPE);
STRING_STATIC(AZURE_QUERY_SIG_STR,                                  "sig");

STRING_STATIC(AZURE_QUERY_VALUE_LIST_STR,                           "list");
STRING_EXTERN(AZURE_QUERY_VALUE_CONTAINER_STR,                      AZURE_QUERY_VALUE_CONTAINER);

/***********************************************************************************************************************************
XML tags
***********************************************************************************************************************************/
STRING_STATIC(AZURE_XML_TAG_BLOB_PREFIX_STR,                        "BlobPrefix");
STRING_STATIC(AZURE_XML_TAG_BLOB_STR,                               "Blob");
STRING_STATIC(AZURE_XML_TAG_BLOBS_STR,                              "Blobs");
STRING_STATIC(AZURE_XML_TAG_CONTENT_LENGTH_STR,                     "Content-Length");
STRING_STATIC(AZURE_XML_TAG_LAST_MODIFIED_STR,                      "Last-Modified");
STRING_STATIC(AZURE_XML_TAG_NEXT_MARKER_STR,                        "NextMarker");
STRING_STATIC(AZURE_XML_TAG_NAME_STR,                               "Name");
STRING_STATIC(AZURE_XML_TAG_PROPERTIES_STR,                         "Properties");

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct StorageAzure
{
    STORAGE_COMMON_MEMBER;
    HttpClient *httpClient;                                         // Http client to service requests
    StringList *headerRedactList;                                   // List of headers to redact from logging
    StringList *queryRedactList;                                    // List of query keys to redact from logging

    const String *container;                                        // Container to store data in
    const String *account;                                          // Account
    const Buffer *sharedKey;                                        // Shared key
    const HttpQuery *sasKey;                                        // SAS key
    const String *host;                                             // Host name
    size_t blockSize;                                               // Block size for multi-block upload
    const String *pathPrefix;                                       // Account/container prefix

    uint64_t fileId;                                                // Id to used to make file block identifiers unique
};

/***********************************************************************************************************************************
Generate authorization header and add it to the supplied header list

Based on the documentation at https://docs.microsoft.com/en-us/rest/api/storageservices/authorize-with-shared-key
***********************************************************************************************************************************/
static void
storageAzureAuth(
    StorageAzure *this, const String *verb, const String *path, HttpQuery *query, const String *dateTime, HttpHeader *httpHeader)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_AZURE, this);
        FUNCTION_TEST_PARAM(STRING, verb);
        FUNCTION_TEST_PARAM(STRING, path);
        FUNCTION_TEST_PARAM(HTTP_QUERY, query);
        FUNCTION_TEST_PARAM(STRING, dateTime);
        FUNCTION_TEST_PARAM(KEY_VALUE, httpHeader);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(verb != NULL);
    ASSERT(path != NULL);
    ASSERT(dateTime != NULL);
    ASSERT(httpHeader != NULL);
    ASSERT(httpHeaderGet(httpHeader, HTTP_HEADER_CONTENT_LENGTH_STR) != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Host header is required for both types of authentication
        httpHeaderPut(httpHeader, HTTP_HEADER_HOST_STR, this->host);

        // Shared key authentication
        if (this->sharedKey != NULL)
        {
            // Set required headers
            httpHeaderPut(httpHeader, HTTP_HEADER_DATE_STR, dateTime);
            httpHeaderPut(httpHeader, AZURE_HEADER_VERSION_STR, AZURE_HEADER_VERSION_VALUE_STR);

            // Generate canonical headers
            String *headerCanonical = strNew();
            StringList *headerKeyList = httpHeaderList(httpHeader);

            for (unsigned int headerKeyIdx = 0; headerKeyIdx < strLstSize(headerKeyList); headerKeyIdx++)
            {
                const String *headerKey = strLstGet(headerKeyList, headerKeyIdx);

                if (!strBeginsWithZ(headerKey, "x-ms-"))
                    continue;

                strCatFmt(headerCanonical, "%s:%s\n", strZ(headerKey), strZ(httpHeaderGet(httpHeader, headerKey)));
            }

            // Generate canonical query
            String *queryCanonical = strNew();

            if (query != NULL)
            {
                StringList *queryKeyList = httpQueryList(query);
                ASSERT(!strLstEmpty(queryKeyList));

                for (unsigned int queryKeyIdx = 0; queryKeyIdx < strLstSize(queryKeyList); queryKeyIdx++)
                {
                    const String *queryKey = strLstGet(queryKeyList, queryKeyIdx);

                    strCatFmt(queryCanonical, "\n%s:%s", strZ(queryKey), strZ(httpQueryGet(query, queryKey)));
                }
            }

            // Generate string to sign
            const String *contentLength = httpHeaderGet(httpHeader, HTTP_HEADER_CONTENT_LENGTH_STR);
            const String *contentMd5 = httpHeaderGet(httpHeader, HTTP_HEADER_CONTENT_MD5_STR);
            const String *const range = httpHeaderGet(httpHeader, HTTP_HEADER_RANGE_STR);

            const String *stringToSign = strNewFmt(
                "%s\n"                                                  // verb
                "\n"                                                    // content-encoding
                "\n"                                                    // content-language
                "%s\n"                                                  // content-length
                "%s\n"                                                  // content-md5
                "\n"                                                    // content-type
                "%s\n"                                                  // date
                "\n"                                                    // If-Modified-Since
                "\n"                                                    // If-Match
                "\n"                                                    // If-None-Match
                "\n"                                                    // If-Unmodified-Since
                "%s\n"                                                  // range
                "%s"                                                    // Canonicalized headers
                "/%s%s"                                                 // Canonicalized account/path
                "%s",                                                   // Canonicalized query
                strZ(verb), strEq(contentLength, ZERO_STR) ? "" : strZ(contentLength), contentMd5 == NULL ? "" : strZ(contentMd5),
                strZ(dateTime), range == NULL ? "" : strZ(range), strZ(headerCanonical), strZ(this->account), strZ(path),
                strZ(queryCanonical));

            // Generate authorization header
            httpHeaderPut(
                httpHeader, HTTP_HEADER_AUTHORIZATION_STR, strNewFmt("SharedKey %s:%s", strZ(this->account),
                strZ(strNewEncode(encodeBase64, cryptoHmacOne(hashTypeSha256, this->sharedKey, BUFSTR(stringToSign))))));
        }
        // SAS authentication
        else
            httpQueryMerge(query, this->sasKey);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_TEST_RETURN_VOID();
}

/***********************************************************************************************************************************
Process Azure request
***********************************************************************************************************************************/
HttpRequest *
storageAzureRequestAsync(StorageAzure *this, const String *verb, StorageAzureRequestAsyncParam param)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STORAGE_AZURE, this);
        FUNCTION_LOG_PARAM(STRING, verb);
        FUNCTION_LOG_PARAM(STRING, param.path);
        FUNCTION_LOG_PARAM(HTTP_HEADER, param.header);
        FUNCTION_LOG_PARAM(HTTP_QUERY, param.query);
        FUNCTION_LOG_PARAM(BUFFER, param.content);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(verb != NULL);

    HttpRequest *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Prepend path prefix
        param.path = param.path == NULL ? this->pathPrefix : strNewFmt("%s%s", strZ(this->pathPrefix), strZ(param.path));

        // Create header list and add content length
        HttpHeader *requestHeader = param.header == NULL ?
            httpHeaderNew(this->headerRedactList) : httpHeaderDup(param.header, this->headerRedactList);

        // Set content length
        httpHeaderAdd(
            requestHeader, HTTP_HEADER_CONTENT_LENGTH_STR,
            param.content == NULL || bufEmpty(param.content) ? ZERO_STR : strNewFmt("%zu", bufUsed(param.content)));

        // Calculate content-md5 header if there is content
        if (param.content != NULL)
        {
            httpHeaderAdd(
                requestHeader, HTTP_HEADER_CONTENT_MD5_STR, strNewEncode(encodeBase64, cryptoHashOne(hashTypeMd5, param.content)));
        }

        // Encode path
        const String *const path = httpUriEncode(param.path, true);

        // Make a copy of the query so it can be modified
        HttpQuery *query =
            this->sasKey != NULL && param.query == NULL ?
                httpQueryNewP(.redactList = this->queryRedactList) :
                httpQueryDupP(param.query, .redactList = this->queryRedactList);

        // Generate authorization header
        storageAzureAuth(this, verb, path, query, httpDateFromTime(time(NULL)), requestHeader);

        // Send request
        MEM_CONTEXT_PRIOR_BEGIN()
        {
            result = httpRequestNewP(
                this->httpClient, verb, path, .query = query, .header = requestHeader, .content = param.content);
        }
        MEM_CONTEXT_END();
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(HTTP_REQUEST, result);
}

HttpResponse *
storageAzureResponse(HttpRequest *request, StorageAzureResponseParam param)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(HTTP_REQUEST, request);
        FUNCTION_LOG_PARAM(BOOL, param.allowMissing);
        FUNCTION_LOG_PARAM(BOOL, param.contentIo);
    FUNCTION_LOG_END();

    ASSERT(request != NULL);

    HttpResponse *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Get response
        result = httpRequestResponse(request, !param.contentIo);

        // Error if the request was not successful
        if (!httpResponseCodeOk(result) && (!param.allowMissing || httpResponseCode(result) != HTTP_RESPONSE_CODE_NOT_FOUND))
            httpRequestError(request, result);

        // Move response to the prior context
        httpResponseMove(result, memContextPrior());
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(HTTP_RESPONSE, result);
}

HttpResponse *
storageAzureRequest(StorageAzure *this, const String *verb, StorageAzureRequestParam param)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STORAGE_AZURE, this);
        FUNCTION_LOG_PARAM(STRING, verb);
        FUNCTION_LOG_PARAM(STRING, param.path);
        FUNCTION_LOG_PARAM(HTTP_HEADER, param.header);
        FUNCTION_LOG_PARAM(HTTP_QUERY, param.query);
        FUNCTION_LOG_PARAM(BUFFER, param.content);
        FUNCTION_LOG_PARAM(BOOL, param.allowMissing);
        FUNCTION_LOG_PARAM(BOOL, param.contentIo);
    FUNCTION_LOG_END();

    HttpRequest *const request = storageAzureRequestAsyncP(
        this, verb, .path = param.path, .header = param.header, .query = param.query, .content = param.content);
    HttpResponse *const result = storageAzureResponseP(request, .allowMissing = param.allowMissing, .contentIo = param.contentIo);

    httpRequestFree(request);

    FUNCTION_LOG_RETURN(HTTP_RESPONSE, result);
}

/***********************************************************************************************************************************
General function for listing files to be used by other list routines
***********************************************************************************************************************************/
static void
storageAzureListInternal(
    StorageAzure *this, const String *path, StorageInfoLevel level, const String *expression, bool recurse,
    StorageListCallback callback, void *callbackData)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STORAGE_AZURE, this);
        FUNCTION_LOG_PARAM(STRING, path);
        FUNCTION_LOG_PARAM(ENUM, level);
        FUNCTION_LOG_PARAM(STRING, expression);
        FUNCTION_LOG_PARAM(BOOL, recurse);
        FUNCTION_LOG_PARAM(FUNCTIONP, callback);
        FUNCTION_LOG_PARAM_P(VOID, callbackData);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(path != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Build the base prefix by stripping off the initial /
        const String *basePrefix;

        if (strSize(path) == 1)
            basePrefix = EMPTY_STR;
        else
            basePrefix = strNewFmt("%s/", strZ(strSub(path, 1)));

        // Get the expression prefix when possible to limit initial results
        const String *expressionPrefix = regExpPrefix(expression);

        // If there is an expression prefix then use it to build the query prefix, otherwise query prefix is base prefix
        const String *queryPrefix;

        if (expressionPrefix == NULL)
            queryPrefix = basePrefix;
        else
        {
            if (strEmpty(basePrefix))
                queryPrefix = expressionPrefix;
            else
                queryPrefix = strNewFmt("%s%s", strZ(basePrefix), strZ(expressionPrefix));
        }

        // Create query
        HttpQuery *query = httpQueryNewP();

        // Add the delimiter to not recurse
        if (!recurse)
            httpQueryAdd(query, AZURE_QUERY_DELIMITER_STR, FSLASH_STR);

        // Add resource type
        httpQueryAdd(query, AZURE_QUERY_RESTYPE_STR, AZURE_QUERY_VALUE_CONTAINER_STR);

        // Add list comp
        httpQueryAdd(query, AZURE_QUERY_COMP_STR, AZURE_QUERY_VALUE_LIST_STR);

        // Don't specify empty prefix because it is the default
        if (!strEmpty(queryPrefix))
            httpQueryAdd(query, AZURE_QUERY_PREFIX_STR, queryPrefix);

        // Loop as long as a continuation marker returned
        HttpRequest *request = NULL;

        do
        {
            // Use an inner mem context here because we could potentially be retrieving millions of files so it is a good idea to
            // free memory at regular intervals
            MEM_CONTEXT_TEMP_BEGIN()
            {
                HttpResponse *response = NULL;

                // If there is an outstanding async request then wait for the response
                if (request != NULL)
                {
                    response = storageAzureResponseP(request);

                    httpRequestFree(request);
                    request = NULL;
                }
                // Else get the response immediately from a sync request
                else
                    response = storageAzureRequestP(this, HTTP_VERB_GET_STR, .query = query);

                XmlNode *xmlRoot = xmlDocumentRoot(xmlDocumentNewBuf(httpResponseContent(response)));

                // If a continuation marker exists then send an async request to get more data
                const String *continuationMarker = xmlNodeContent(xmlNodeChild(xmlRoot, AZURE_XML_TAG_NEXT_MARKER_STR, false));

                if (!strEq(continuationMarker, EMPTY_STR))
                {
                    httpQueryPut(query, AZURE_QUERY_MARKER_STR, continuationMarker);

                    // Store request in the outer temp context
                    MEM_CONTEXT_PRIOR_BEGIN()
                    {
                        request = storageAzureRequestAsyncP(this, HTTP_VERB_GET_STR, .query = query);
                    }
                    MEM_CONTEXT_PRIOR_END();
                }

                // Get prefix list
                XmlNode *blobs = xmlNodeChild(xmlRoot, AZURE_XML_TAG_BLOBS_STR, true);
                XmlNodeList *blobPrefixList = xmlNodeChildList(blobs, AZURE_XML_TAG_BLOB_PREFIX_STR);

                for (unsigned int blobPrefixIdx = 0; blobPrefixIdx < xmlNodeLstSize(blobPrefixList); blobPrefixIdx++)
                {
                    const XmlNode *subPathNode = xmlNodeLstGet(blobPrefixList, blobPrefixIdx);

                    // Get path name
                    StorageInfo info =
                    {
                        .level = level,
                        .name = xmlNodeContent(xmlNodeChild(subPathNode, AZURE_XML_TAG_NAME_STR, true)),
                        .exists = true,
                    };

                    // Strip off base prefix and final /
                    info.name = strSubN(info.name, strSize(basePrefix), strSize(info.name) - strSize(basePrefix) - 1);

                    // Add type info if requested
                    if (level >= storageInfoLevelType)
                        info.type = storageTypePath;

                    // Callback with info
                    callback(callbackData, &info);
                }

                // Get file list
                XmlNodeList *fileList = xmlNodeChildList(blobs, AZURE_XML_TAG_BLOB_STR);

                for (unsigned int fileIdx = 0; fileIdx < xmlNodeLstSize(fileList); fileIdx++)
                {
                    const XmlNode *fileNode = xmlNodeLstGet(fileList, fileIdx);

                    // Get file name
                    StorageInfo info =
                    {
                        .level = level,
                        .name = xmlNodeContent(xmlNodeChild(fileNode, AZURE_XML_TAG_NAME_STR, true)),
                        .exists = true,
                    };

                    // Strip off the base prefix when present
                    if (!strEmpty(basePrefix))
                        info.name = strSub(info.name, strSize(basePrefix));

                    // Add basic info if requested (no need to add type info since file is default type)
                    if (level >= storageInfoLevelBasic)
                    {
                        XmlNode *property = xmlNodeChild(fileNode, AZURE_XML_TAG_PROPERTIES_STR, true);

                        info.size = cvtZToUInt64(
                            strZ(xmlNodeContent(xmlNodeChild(property, AZURE_XML_TAG_CONTENT_LENGTH_STR, true))));
                        info.timeModified = httpDateToTime(
                            xmlNodeContent(xmlNodeChild(property, AZURE_XML_TAG_LAST_MODIFIED_STR, true)));
                    }

                    // Callback with info
                    callback(callbackData, &info);
                }
            }
            MEM_CONTEXT_TEMP_END();
        }
        while (request != NULL);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
static StorageInfo
storageAzureInfo(THIS_VOID, const String *file, StorageInfoLevel level, StorageInterfaceInfoParam param)
{
    THIS(StorageAzure);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_AZURE, this);
        FUNCTION_LOG_PARAM(STRING, file);
        FUNCTION_LOG_PARAM(ENUM, level);
        (void)param;                                                // No parameters are used
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(file != NULL);

    // Attempt to get file info
    HttpResponse *const httpResponse = storageAzureRequestP(this, HTTP_VERB_HEAD_STR, .path = file, .allowMissing = true);

    // Does the file exist?
    StorageInfo result = {.level = level, .exists = httpResponseCodeOk(httpResponse)};

    // Add basic level info if requested and the file exists (no need to add type info since file is default type)
    if (result.level >= storageInfoLevelBasic && result.exists)
    {
        result.size = cvtZToUInt64(strZ(httpHeaderGet(httpResponseHeader(httpResponse), HTTP_HEADER_CONTENT_LENGTH_STR)));
        result.timeModified = httpDateToTime(httpHeaderGet(httpResponseHeader(httpResponse), HTTP_HEADER_LAST_MODIFIED_STR));
    }

    httpResponseFree(httpResponse);

    FUNCTION_LOG_RETURN(STORAGE_INFO, result);
}

/**********************************************************************************************************************************/
static void
storageAzureListCallback(void *const callbackData, const StorageInfo *const info)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM_P(VOID, callbackData);
        FUNCTION_TEST_PARAM(STORAGE_INFO, info);
    FUNCTION_TEST_END();

    ASSERT(callbackData != NULL);
    ASSERT(info != NULL);

    storageLstAdd(callbackData, info);

    FUNCTION_TEST_RETURN_VOID();
}

static StorageList *
storageAzureList(THIS_VOID, const String *const path, const StorageInfoLevel level, const StorageInterfaceListParam param)
{
    THIS(StorageAzure);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_AZURE, this);
        FUNCTION_LOG_PARAM(STRING, path);
        FUNCTION_LOG_PARAM(ENUM, level);
        FUNCTION_LOG_PARAM(STRING, param.expression);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(path != NULL);

    StorageList *const result = storageLstNew(level);

    storageAzureListInternal(this, path, level, param.expression, false, storageAzureListCallback, result);

    FUNCTION_LOG_RETURN(STORAGE_LIST, result);
}

/**********************************************************************************************************************************/
static StorageRead *
storageAzureNewRead(THIS_VOID, const String *file, bool ignoreMissing, StorageInterfaceNewReadParam param)
{
    THIS(StorageAzure);

    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STORAGE_AZURE, this);
        FUNCTION_LOG_PARAM(STRING, file);
        FUNCTION_LOG_PARAM(BOOL, ignoreMissing);
        FUNCTION_LOG_PARAM(UINT64, param.offset);
        FUNCTION_LOG_PARAM(VARIANT, param.limit);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(file != NULL);

    FUNCTION_LOG_RETURN(STORAGE_READ, storageReadAzureNew(this, file, ignoreMissing, param.offset, param.limit));
}

/**********************************************************************************************************************************/
static StorageWrite *
storageAzureNewWrite(THIS_VOID, const String *file, StorageInterfaceNewWriteParam param)
{
    THIS(StorageAzure);

    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STORAGE_AZURE, this);
        FUNCTION_LOG_PARAM(STRING, file);
        (void)param;                                                // No parameters are used
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(file != NULL);
    ASSERT(param.createPath);
    ASSERT(param.user == NULL);
    ASSERT(param.group == NULL);
    ASSERT(param.timeModified == 0);

    FUNCTION_LOG_RETURN(STORAGE_WRITE, storageWriteAzureNew(this, file, this->fileId++, this->blockSize));
}

/**********************************************************************************************************************************/
typedef struct StorageAzurePathRemoveData
{
    StorageAzure *this;                                             // Storage object
    MemContext *memContext;                                         // Mem context to create requests in
    HttpRequest *request;                                           // Async remove request
    const String *path;                                             // Root path of remove
} StorageAzurePathRemoveData;

static void
storageAzurePathRemoveCallback(void *const callbackData, const StorageInfo *const info)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM_P(VOID, callbackData);
        FUNCTION_TEST_PARAM(STORAGE_INFO, info);
    FUNCTION_TEST_END();

    ASSERT(callbackData != NULL);
    ASSERT(info != NULL);

    StorageAzurePathRemoveData *const data = callbackData;

    // Get response from prior async request
    if (data->request != NULL)
    {
        httpResponseFree(storageAzureResponseP(data->request, .allowMissing = true));
        httpRequestFree(data->request);
        data->request = NULL;
    }

    // Only delete files since paths don't really exist
    if (info->type == storageTypeFile)
    {
        MEM_CONTEXT_BEGIN(data->memContext)
        {
            data->request = storageAzureRequestAsyncP(
                data->this, HTTP_VERB_DELETE_STR, strNewFmt("%s/%s", strZ(data->path), strZ(info->name)));
        }
        MEM_CONTEXT_END();
    }

    FUNCTION_TEST_RETURN_VOID();
}

static bool
storageAzurePathRemove(THIS_VOID, const String *path, bool recurse, StorageInterfacePathRemoveParam param)
{
    THIS(StorageAzure);

    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STORAGE_AZURE, this);
        FUNCTION_LOG_PARAM(STRING, path);
        FUNCTION_LOG_PARAM(BOOL, recurse);
        (void)param;                                                // No parameters are used
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(path != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        StorageAzurePathRemoveData data =
        {
            .this = this,
            .memContext = memContextCurrent(),
            .path = strEq(path, FSLASH_STR) ? EMPTY_STR : path,
        };

        storageAzureListInternal(this, path, storageInfoLevelType, NULL, true, storageAzurePathRemoveCallback, &data);

        // Check response on last async request
        if (data.request != NULL)
            storageAzureResponseP(data.request, .allowMissing = true);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(BOOL, true);
}

/**********************************************************************************************************************************/
static void
storageAzureRemove(THIS_VOID, const String *file, StorageInterfaceRemoveParam param)
{
    THIS(StorageAzure);

    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STORAGE_AZURE, this);
        FUNCTION_LOG_PARAM(STRING, file);
        FUNCTION_LOG_PARAM(BOOL, param.errorOnMissing);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(file != NULL);
    ASSERT(!param.errorOnMissing);

    httpResponseFree(storageAzureRequestP(this, HTTP_VERB_DELETE_STR, file, .allowMissing = true));

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
static const StorageInterface storageInterfaceAzure =
{
    .info = storageAzureInfo,
    .list = storageAzureList,
    .newRead = storageAzureNewRead,
    .newWrite = storageAzureNewWrite,
    .pathRemove = storageAzurePathRemove,
    .remove = storageAzureRemove,
};

Storage *
storageAzureNew(
    const String *const path, const bool write, StoragePathExpressionCallback pathExpressionFunction, const String *const container,
    const String *const account, const StorageAzureKeyType keyType, const String *const key, const size_t blockSize,
    const String *const endpoint, const StorageAzureUriStyle uriStyle, const unsigned int port, const TimeMSec timeout,
    const bool verifyPeer, const String *const caFile, const String *const caPath)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STRING, path);
        FUNCTION_LOG_PARAM(BOOL, write);
        FUNCTION_LOG_PARAM(FUNCTIONP, pathExpressionFunction);
        FUNCTION_LOG_PARAM(STRING, container);
        FUNCTION_TEST_PARAM(STRING, account);
        FUNCTION_LOG_PARAM(STRING_ID, keyType);
        FUNCTION_TEST_PARAM(STRING, key);
        FUNCTION_LOG_PARAM(SIZE, blockSize);
        FUNCTION_LOG_PARAM(STRING, endpoint);
        FUNCTION_LOG_PARAM(ENUM, uriStyle);
        FUNCTION_LOG_PARAM(UINT, port);
        FUNCTION_LOG_PARAM(TIME_MSEC, timeout);
        FUNCTION_LOG_PARAM(BOOL, verifyPeer);
        FUNCTION_LOG_PARAM(STRING, caFile);
        FUNCTION_LOG_PARAM(STRING, caPath);
    FUNCTION_LOG_END();

    ASSERT(path != NULL);
    ASSERT(container != NULL);
    ASSERT(account != NULL);
    ASSERT(endpoint != NULL);
    ASSERT(key != NULL);
    ASSERT(blockSize != 0);

    Storage *this = NULL;

    OBJ_NEW_BEGIN(StorageAzure, .childQty = MEM_CONTEXT_QTY_MAX, .allocQty = MEM_CONTEXT_QTY_MAX)
    {
        StorageAzure *driver = OBJ_NEW_ALLOC();

        *driver = (StorageAzure)
        {
            .interface = storageInterfaceAzure,
            .container = strDup(container),
            .account = strDup(account),
            .blockSize = blockSize,
            .host = uriStyle == storageAzureUriStyleHost ? strNewFmt("%s.%s", strZ(account), strZ(endpoint)) : strDup(endpoint),
            .pathPrefix = uriStyle == storageAzureUriStyleHost ?
                strNewFmt("/%s", strZ(container)) : strNewFmt("/%s/%s", strZ(account), strZ(container)),
        };

        // Store shared key or parse sas query
        if (keyType == storageAzureKeyTypeShared)
            driver->sharedKey = bufNewDecode(encodeBase64, key);
        else
            driver->sasKey = httpQueryNewStr(key);

        // Create the http client used to service requests
        driver->httpClient = httpClientNew(
            tlsClientNewP(
                sckClientNew(driver->host, port, timeout, timeout), driver->host, timeout, timeout, verifyPeer, .caFile = caFile,
                .caPath = caPath),
            timeout);

        // Create list of redacted headers
        driver->headerRedactList = strLstNew();
        strLstAdd(driver->headerRedactList, HTTP_HEADER_AUTHORIZATION_STR);
        strLstAdd(driver->headerRedactList, HTTP_HEADER_DATE_STR);

        // Create list of redacted query keys
        driver->queryRedactList = strLstNew();
        strLstAdd(driver->queryRedactList, AZURE_QUERY_SIG_STR);

        // Generate starting file id
        cryptoRandomBytes((unsigned char *)&driver->fileId, sizeof(driver->fileId));

        this = storageNew(STORAGE_AZURE_TYPE, path, 0, 0, write, pathExpressionFunction, driver, driver->interface);
    }
    OBJ_NEW_END();

    FUNCTION_LOG_RETURN(STORAGE, this);
}
