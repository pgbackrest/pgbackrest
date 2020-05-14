/***********************************************************************************************************************************
Azure Storage
***********************************************************************************************************************************/
#include "build.auto.h"

#include <string.h>

#include "common/crypto/hash.h"
#include "common/encode.h"
#include "common/debug.h"
#include "common/io/http/cache.h"
#include "common/io/http/common.h"
#include "common/log.h"
#include "common/memContext.h"
#include "common/regExp.h"
#include "common/type/object.h"
#include "common/type/xml.h"
#include "storage/azure/read.h"
#include "storage/azure/storage.intern.h"
#include "storage/azure/write.h"

/***********************************************************************************************************************************
Storage type
***********************************************************************************************************************************/
STRING_EXTERN(STORAGE_AZURE_TYPE_STR,                               STORAGE_AZURE_TYPE);

/***********************************************************************************************************************************
Azure http headers
***********************************************************************************************************************************/
STRING_STATIC(AZURE_HEADER_VERSION_STR,                             "x-ms-version");
STRING_STATIC(AZURE_HEADER_VERSION_VALUE_STR,                       "2019-02-02");

/***********************************************************************************************************************************
Azure query tokens
***********************************************************************************************************************************/
STRING_STATIC(AZURE_QUERY_MARKER_STR,                               "marker");
STRING_STATIC(AZURE_QUERY_COMP_STR,                                 "comp");
STRING_STATIC(AZURE_QUERY_DELIMITER_STR,                            "delimiter");
STRING_STATIC(AZURE_QUERY_PREFIX_STR,                               "prefix");
STRING_STATIC(AZURE_QUERY_RESTYPE_STR,                              "restype");

STRING_STATIC(AZURE_QUERY_VALUE_LIST_STR,                           "list");
STRING_STATIC(AZURE_QUERY_VALUE_CONTAINER_STR,                      "container");

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
    MemContext *memContext;
    HttpClientCache *httpClientCache;                               // Http client cache to service requests
    StringList *headerRedactList;                                   // List of headers to redact from logging

    const String *container;                                        // Container to store data in
    const String *account;                                          // Account
    const String *key;                                              // Shared Secret Key
    const String *host;                                             // Host name
    size_t partSize;                                                // Part size for multi-part upload
    const String *uriPrefix;                                        // Account/container prefix
};

/***********************************************************************************************************************************
Expected ISO-8601 data/time size
***********************************************************************************************************************************/
#define ISO_8601_DATE_TIME_SIZE                                     16

/***********************************************************************************************************************************
Format ISO-8601 date/time for authentication
***********************************************************************************************************************************/
static String *
storageAzureDateTime(time_t authTime)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(TIME, authTime);
    FUNCTION_TEST_END();

    char buffer[ISO_8601_DATE_TIME_SIZE + 1];

    THROW_ON_SYS_ERROR(
        strftime(buffer, sizeof(buffer), "%Y%m%dT%H%M%SZ", gmtime(&authTime)) != ISO_8601_DATE_TIME_SIZE, AssertError,
        "unable to format date");

    FUNCTION_TEST_RETURN(strNew(buffer));
}

/***********************************************************************************************************************************
Generate authorization header and add it to the supplied header list

Based on the documentation at https://docs.microsoft.com/en-us/rest/api/storageservices/authorize-with-shared-key
***********************************************************************************************************************************/
static void
storageAzureAuth(
    StorageAzure *this, const String *verb, const String *uri, const HttpQuery *query, const String *dateTime,
    HttpHeader *httpHeader)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_AZURE, this);
        FUNCTION_TEST_PARAM(STRING, verb);
        FUNCTION_TEST_PARAM(STRING, uri);
        FUNCTION_TEST_PARAM(HTTP_QUERY, query);
        FUNCTION_TEST_PARAM(STRING, dateTime);
        FUNCTION_TEST_PARAM(KEY_VALUE, httpHeader);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(verb != NULL);
    ASSERT(uri != NULL);
    ASSERT(dateTime != NULL);
    ASSERT(httpHeader != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Set required headers
        httpHeaderPut(httpHeader, HTTP_HEADER_DATE_STR, dateTime);
        httpHeaderPut(httpHeader, HTTP_HEADER_HOST_STR, this->host);
        httpHeaderPut(httpHeader, AZURE_HEADER_VERSION_STR, AZURE_HEADER_VERSION_VALUE_STR);

        // Generate canonical headers
        String *headerCanonical = strNew("");
        StringList *headerKeyList = httpHeaderList(httpHeader);

        for (unsigned int headerKeyIdx = 0; headerKeyIdx < strLstSize(headerKeyList); headerKeyIdx++)
        {
            const String *headerKey = strLstGet(headerKeyList, headerKeyIdx);

            if (!strBeginsWithZ(headerKey, "x-ms-"))
                continue;

            strCatFmt(headerCanonical, "%s:%s\n", strPtr(headerKey), strPtr(httpHeaderGet(httpHeader, headerKey)));
        }

        // Generate canonical query
        String *queryCanonical = strNew("");

        if (query != NULL)
        {
            StringList *queryKeyList = httpQueryList(query);
            ASSERT(strLstSize(queryKeyList) > 0);

            for (unsigned int queryKeyIdx = 0; queryKeyIdx < strLstSize(queryKeyList); queryKeyIdx++)
            {
                const String *queryKey = strLstGet(queryKeyList, queryKeyIdx);

                strCatFmt(queryCanonical, "\n%s:%s", strPtr(queryKey), strPtr(httpQueryGet(query, queryKey)));
            }
        }

        // Generate string to sign
        const String *contentLength = httpHeaderGet(httpHeader, HTTP_HEADER_CONTENT_LENGTH_STR);
        const String *contentMd5 = httpHeaderGet(httpHeader, HTTP_HEADER_CONTENT_MD5_STR);

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
            "\n"                                                    // range
            "%s"                                                    // Canonicalized headers
            "/%s%s"                                                 // Canonicalized account/uri
            "%s",                                                   // Canonicalized query
            strPtr(verb), strEq(contentLength, ZERO_STR) ? "" : strPtr(contentLength), contentMd5 == NULL ? "" : strPtr(contentMd5),
            strPtr(dateTime), strPtr(headerCanonical), strPtr(this->account), strPtr(uri), strPtr(queryCanonical));

        // !!! Need to actually process above
        (void)query;

        // Generate authorization header
        Buffer *keyBin = bufNew(decodeToBinSize(encodeBase64, strPtr(this->key)));
        decodeToBin(encodeBase64, strPtr(this->key), bufPtr(keyBin));
        bufUsedSet(keyBin, bufSize(keyBin));

        char authHmacBase64[45];
        encodeToStr(
            encodeBase64, bufPtr(cryptoHmacOne(HASH_TYPE_SHA256_STR, keyBin, BUFSTR(stringToSign))),
            HASH_TYPE_SHA256_SIZE, authHmacBase64);

        httpHeaderPut(
            httpHeader, HTTP_HEADER_AUTHORIZATION_STR, strNewFmt("SharedKey %s:%s", strPtr(this->account), authHmacBase64));
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_TEST_RETURN_VOID();
}

/***********************************************************************************************************************************
Process Azure request
***********************************************************************************************************************************/
StorageAzureRequestResult
storageAzureRequest(StorageAzure *this, const String *verb, StorageAzureRequestParam param)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STORAGE_AZURE, this);
        FUNCTION_LOG_PARAM(STRING, verb);
        FUNCTION_LOG_PARAM(STRING, param.uri);
        FUNCTION_LOG_PARAM(HTTP_HEADER, param.header);
        FUNCTION_LOG_PARAM(HTTP_QUERY, param.query);
        FUNCTION_LOG_PARAM(BUFFER, param.body);
        FUNCTION_LOG_PARAM(BOOL, param.returnContent);
        FUNCTION_LOG_PARAM(BOOL, param.allowMissing);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(verb != NULL);

    StorageAzureRequestResult result = {0};
    // unsigned int retryRemaining = 2;
    bool done;

    do
    {
        done = true;

        MEM_CONTEXT_TEMP_BEGIN()
        {
            // Prepend uri prefix
            param.uri = param.uri == NULL ? this->uriPrefix : strNewFmt("%s%s", strPtr(this->uriPrefix), strPtr(param.uri));

            // Create header list and add content length
            HttpHeader *requestHeader = param.header == NULL ?
                httpHeaderNew(this->headerRedactList) : httpHeaderDup(param.header, this->headerRedactList);

            // Set content length
            httpHeaderAdd(
                requestHeader, HTTP_HEADER_CONTENT_LENGTH_STR,
                param.body == NULL || bufUsed(param.body) == 0 ? ZERO_STR : strNewFmt("%zu", bufUsed(param.body)));

            // Calculate content-md5 header if there is content
            if (param.body != NULL)
            {
                char md5Hash[HASH_TYPE_MD5_SIZE_HEX];
                encodeToStr(encodeBase64, bufPtr(cryptoHashOne(HASH_TYPE_MD5_STR, param.body)), HASH_TYPE_M5_SIZE, md5Hash);
                httpHeaderAdd(requestHeader, HTTP_HEADER_CONTENT_MD5_STR, STR(md5Hash));
            }

            // Generate authorization header
            storageAzureAuth(
                this, verb, httpUriEncode(param.uri, true), param.query, storageAzureDateTime(time(NULL)), requestHeader);

            // Get an http client
            HttpClient *httpClient = httpClientCacheGet(this->httpClientCache);

            // Process request
            Buffer *response = httpClientRequest(
                httpClient, verb, param.uri, param.query, requestHeader, param.body, param.returnContent);

            // Error if the request was not successful
            if (!httpClientResponseCodeOk(httpClient) &&
                (!param.allowMissing || httpClientResponseCode(httpClient) != HTTP_RESPONSE_CODE_NOT_FOUND))
            {
                // // If there are retries remaining and a response parse it as XML to extract the Azure error code
                // if (response != NULL && retryRemaining > 0)
                // {
                //     // Attempt to parse the XML and extract the Azure error code
                //     TRY_BEGIN()
                //     {
                //         XmlNode *error = xmlDocumentRoot(xmlDocumentNewBuf(response));
                //         const String *errorCode = xmlNodeContent(xmlNodeChild(error, AZURE_XML_TAG_CODE_STR, true));
                //
                //         if (strEq(errorCode, AZURE_ERROR_REQUEST_TIME_TOO_SKEWED_STR))
                //         {
                //             LOG_DEBUG_FMT(
                //                 "retry %s: %s", strPtr(errorCode),
                //                 strPtr(xmlNodeContent(xmlNodeChild(error, AZURE_XML_TAG_MESSAGE_STR, true))));
                //
                //             retryRemaining--;
                //             done = false;
                //         }
                //     }
                //     // On failure just drop through and report the error as usual
                //     CATCH_ANY()
                //     {
                //     }
                //     TRY_END();
                // }

                // If not done then retry instead of reporting the error
                if (done)
                {
                    // General error message
                    String *error = strNewFmt(
                        "Azure request failed with %u: %s", httpClientResponseCode(httpClient),
                        strPtr(httpClientResponseMessage(httpClient)));

                    // Output uri/query
                    strCat(error, "\n*** URI/Query ***:");

                    strCatFmt(error, "\n%s %s", strPtr(verb), strPtr(httpUriEncode(param.uri, true)));

                    if (param.query != NULL)
                        strCatFmt(error, "?%s", strPtr(httpQueryRender(param.query)));

                    // Output request headers
                    const StringList *requestHeaderList = httpHeaderList(requestHeader);

                    strCat(error, "\n*** Request Headers ***:");

                    for (unsigned int requestHeaderIdx = 0; requestHeaderIdx < strLstSize(requestHeaderList); requestHeaderIdx++)
                    {
                        const String *key = strLstGet(requestHeaderList, requestHeaderIdx);

                        strCatFmt(
                            error, "\n%s: %s", strPtr(key),
                            httpHeaderRedact(requestHeader, key) || strEq(key, HTTP_HEADER_DATE_STR) ?
                                "<redacted>" : strPtr(httpHeaderGet(requestHeader, key)));
                    }

                    // Output response headers
                    const HttpHeader *responseHeader = httpClientResponseHeader(httpClient);
                    const StringList *responseHeaderList = httpHeaderList(responseHeader);

                    if (strLstSize(responseHeaderList) > 0)
                    {
                        strCat(error, "\n*** Response Headers ***:");

                        for (unsigned int responseHeaderIdx = 0; responseHeaderIdx < strLstSize(responseHeaderList);
                                responseHeaderIdx++)
                        {
                            const String *key = strLstGet(responseHeaderList, responseHeaderIdx);
                            strCatFmt(error, "\n%s: %s", strPtr(key), strPtr(httpHeaderGet(responseHeader, key)));
                        }
                    }

                    // If there was content then output it
                    if (response!= NULL)
                        strCatFmt(error, "\n*** Response Content ***:\n%s", strPtr(strNewBuf(response)));

                    THROW(ProtocolError, strPtr(error));
                }
            }
            else
            {
                // On success move the buffer to the prior context
                result.httpClient = httpClient;
                result.responseHeader = httpHeaderMove(
                    httpHeaderDup(httpClientResponseHeader(httpClient), NULL), memContextPrior());
                result.response = bufMove(response, memContextPrior());
            }

        }
        MEM_CONTEXT_TEMP_END();
    }
    while (!done);

    FUNCTION_LOG_RETURN(STORAGE_AZURE_REQUEST_RESULT, result);
}

/***********************************************************************************************************************************
General function for listing files to be used by other list routines
***********************************************************************************************************************************/
static void
storageAzureListInternal(
    StorageAzure *this, const String *path, const String *expression, bool recurse,
    void (*callback)(StorageAzure *this, void *callbackData, const String *name, StorageType type, const XmlNode *xml),
    void *callbackData)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STORAGE_AZURE, this);
        FUNCTION_LOG_PARAM(STRING, path);
        FUNCTION_LOG_PARAM(STRING, expression);
        FUNCTION_LOG_PARAM(BOOL, recurse);
        FUNCTION_LOG_PARAM(FUNCTIONP, callback);
        FUNCTION_LOG_PARAM_P(VOID, callbackData);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(path != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        const String *marker = NULL;

        // Build the base prefix by stripping off the initial /
        const String *basePrefix;

        if (strSize(path) == 1)
            basePrefix = EMPTY_STR;
        else
            basePrefix = strNewFmt("%s/", strPtr(strSub(path, 1)));

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
                queryPrefix = strNewFmt("%s%s", strPtr(basePrefix), strPtr(expressionPrefix));
        }

        // Loop as long as a continuation token returned
        do
        {
            // Use an inner mem context here because we could potentially be retrieving millions of files so it is a good idea to
            // free memory at regular intervals
            MEM_CONTEXT_TEMP_BEGIN()
            {
                HttpQuery *query = httpQueryNew();

                // Add continuation token from the prior loop if any
                if (marker != NULL)
                    httpQueryAdd(query, AZURE_QUERY_MARKER_STR, marker);

                // Add the delimiter to not recurse
                if (!recurse)
                    httpQueryAdd(query, AZURE_QUERY_DELIMITER_STR, FSLASH_STR);

                // Add resource type
                httpQueryAdd(query, AZURE_QUERY_RESTYPE_STR, AZURE_QUERY_VALUE_CONTAINER_STR);

                // Add list comp
                httpQueryAdd(query, AZURE_QUERY_COMP_STR, AZURE_QUERY_VALUE_LIST_STR);

                // Don't specified empty prefix because it is the default
                if (!strEmpty(queryPrefix))
                    httpQueryAdd(query, AZURE_QUERY_PREFIX_STR, queryPrefix);

                XmlNode *xmlRoot = xmlDocumentRoot(
                    xmlDocumentNewBuf(
                        storageAzureRequestP(this, HTTP_VERB_GET_STR, .query = query, .returnContent = true).response));

                // Get subpath list
                XmlNode *blobs = xmlNodeChild(xmlRoot, AZURE_XML_TAG_BLOBS_STR, true);
                XmlNodeList *blobPrefixList = xmlNodeChildList(blobs, AZURE_XML_TAG_BLOB_PREFIX_STR);

                for (unsigned int blobPrefixIdx = 0; blobPrefixIdx < xmlNodeLstSize(blobPrefixList); blobPrefixIdx++)
                {
                    const XmlNode *subPathNode = xmlNodeLstGet(blobPrefixList, blobPrefixIdx);

                    // Get subpath name
                    const String *subPath = xmlNodeContent(xmlNodeChild(subPathNode, AZURE_XML_TAG_NAME_STR, true));

                    // Strip off base prefix and final /
                    subPath = strSubN(subPath, strSize(basePrefix), strSize(subPath) - strSize(basePrefix) - 1);

                    // Add to list
                    callback(this, callbackData, subPath, storageTypePath, NULL);
                }

                // Get file list
                XmlNodeList *fileList = xmlNodeChildList(blobs, AZURE_XML_TAG_BLOB_STR);

                for (unsigned int fileIdx = 0; fileIdx < xmlNodeLstSize(fileList); fileIdx++)
                {
                    const XmlNode *fileNode = xmlNodeLstGet(fileList, fileIdx);

                    // Get file name
                    const String *file = xmlNodeContent(xmlNodeChild(fileNode, AZURE_XML_TAG_NAME_STR, true));

                    // Strip off the base prefix when present
                    file = strEmpty(basePrefix) ? file : strSub(file, strSize(basePrefix));

                    // Add to list
                    callback(
                        this, callbackData, file, storageTypeFile, xmlNodeChild(fileNode, AZURE_XML_TAG_PROPERTIES_STR, true));
                }

                // Get the continuation token and store it in the outer temp context
                MEM_CONTEXT_PRIOR_BEGIN()
                {
                    marker = xmlNodeContent(xmlNodeChild(xmlRoot, AZURE_XML_TAG_NEXT_MARKER_STR, false));

                    // THROW_FMT(AssertError, "MARKER %s", strPtr(marker));
                }
                MEM_CONTEXT_PRIOR_END();
            }
            MEM_CONTEXT_TEMP_END();
        }
        while (!strEq(marker, EMPTY_STR));
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
    StorageAzureRequestResult httpResult = storageAzureRequestP(
        this, HTTP_VERB_HEAD_STR, .uri = file, .returnContent = true, .allowMissing = true);

    // Does the file exist?
    StorageInfo result = {.level = level, .exists = httpClientResponseCodeOk(httpResult.httpClient)};

    // Add basic level info if requested and the file exists
    if (result.level >= storageInfoLevelBasic && result.exists)
    {
        result.type = storageTypeFile;
        result.size = cvtZToUInt64(strPtr(httpHeaderGet(httpResult.responseHeader, HTTP_HEADER_CONTENT_LENGTH_STR)));
        result.timeModified = httpLastModifiedToTime(httpHeaderGet(httpResult.responseHeader, HTTP_HEADER_LAST_MODIFIED_STR));
    }

    FUNCTION_LOG_RETURN(STORAGE_INFO, result);
}

/**********************************************************************************************************************************/
typedef struct StorageAzureInfoListData
{
    StorageInfoLevel level;                                         // Level of info to set
    StorageInfoListCallback callback;                               // User-supplied callback function
    void *callbackData;                                             // User-supplied callback data
} StorageAzureInfoListData;

static void
storageAzureInfoListCallback(StorageAzure *this, void *callbackData, const String *name, StorageType type, const XmlNode *xml)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_AZURE, this);
        FUNCTION_TEST_PARAM_P(VOID, callbackData);
        FUNCTION_TEST_PARAM(STRING, name);
        FUNCTION_TEST_PARAM(ENUM, type);
        FUNCTION_TEST_PARAM(XML_NODE, xml);
    FUNCTION_TEST_END();

    (void)this;
    ASSERT(callbackData != NULL);
    ASSERT(name != NULL);
    ASSERT((type == storageTypePath && xml == NULL) || (type == storageTypeFile && xml != NULL));

    StorageAzureInfoListData *data = (StorageAzureInfoListData *)callbackData;

    StorageInfo info =
    {
        .name = name,
        .level = data->level,
        .exists = true,
    };

    if (data->level >= storageInfoLevelBasic)
    {
        info.type = type;
        info.size = type == storageTypeFile ?
            cvtZToUInt64(strPtr(xmlNodeContent(xmlNodeChild(xml, AZURE_XML_TAG_CONTENT_LENGTH_STR, true)))) : 0;
        info.timeModified = type == storageTypeFile ?
            httpLastModifiedToTime(xmlNodeContent(xmlNodeChild(xml, AZURE_XML_TAG_LAST_MODIFIED_STR, true))) : 0;
    }

    data->callback(data->callbackData, &info);

    FUNCTION_TEST_RETURN_VOID();
}

static bool
storageAzureInfoList(
    THIS_VOID, const String *path, StorageInfoLevel level, StorageInfoListCallback callback, void *callbackData,
    StorageInterfaceInfoListParam param)
{
    THIS(StorageAzure);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_AZURE, this);
        FUNCTION_LOG_PARAM(STRING, path);
        FUNCTION_LOG_PARAM(ENUM, level);
        FUNCTION_LOG_PARAM(FUNCTIONP, callback);
        FUNCTION_LOG_PARAM_P(VOID, callbackData);
        FUNCTION_LOG_PARAM(STRING, param.expression);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(path != NULL);
    ASSERT(callback != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        StorageAzureInfoListData data = {.level = level, .callback = callback, .callbackData = callbackData};
        storageAzureListInternal(this, path, param.expression, false, storageAzureInfoListCallback, &data);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(BOOL, true);
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
        (void)param;                                                // No parameters are used
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(file != NULL);

    FUNCTION_LOG_RETURN(STORAGE_READ, storageReadAzureNew(this, file, ignoreMissing));
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

    FUNCTION_LOG_RETURN(STORAGE_WRITE, storageWriteAzureNew(this, file, this->partSize));
}

/**********************************************************************************************************************************/
typedef struct StorageAzurePathRemoveData
{
    const String *path;
    // MemContext *memContext;                                         // Mem context to create xml document in
    // unsigned int size;                                              // Size of delete request
    // XmlDocument *xml;                                               // Delete request
} StorageAzurePathRemoveData;

// static void
// storageAzurePathRemoveInternal(StorageAzure *this, XmlDocument *request)
// {
//     FUNCTION_TEST_BEGIN();
//         FUNCTION_TEST_PARAM(STORAGE_AZURE, this);
//         FUNCTION_TEST_PARAM(XML_DOCUMENT, request);
//     FUNCTION_TEST_END();
//
//     ASSERT(this != NULL);
//     ASSERT(request != NULL);
//
//     Buffer *response = storageAzureRequest(
//         this, HTTP_VERB_POST_STR, FSLASH_STR, NULL, httpQueryAdd(httpQueryNew(), AZURE_QUERY_DELETE_STR, EMPTY_STR),
//         xmlDocumentBuf(request), true, false).response;
//
//     // Nothing is returned when there are no errors
//     if (response != NULL)
//     {
//         XmlNodeList *errorList = xmlNodeChildList(xmlDocumentRoot(xmlDocumentNewBuf(response)), AZURE_XML_TAG_ERROR_STR);
//
//         if (xmlNodeLstSize(errorList) > 0)
//         {
//             XmlNode *error = xmlNodeLstGet(errorList, 0);
//
//             THROW_FMT(
//                 FileRemoveError, STORAGE_ERROR_PATH_REMOVE_FILE ": [%s] %s",
//                 strPtr(xmlNodeContent(xmlNodeChild(error, AZURE_XML_TAG_KEY_STR, true))),
//                 strPtr(xmlNodeContent(xmlNodeChild(error, AZURE_XML_TAG_CODE_STR, true))),
//                 strPtr(xmlNodeContent(xmlNodeChild(error, AZURE_XML_TAG_MESSAGE_STR, true))));
//         }
//     }
//
//     FUNCTION_TEST_RETURN_VOID();
// }

static void
storageAzurePathRemoveCallback(StorageAzure *this, void *callbackData, const String *name, StorageType type, const XmlNode *xml)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_AZURE, this);
        FUNCTION_TEST_PARAM_P(VOID, callbackData);
        FUNCTION_TEST_PARAM(STRING, name);
        FUNCTION_TEST_PARAM(ENUM, type);
        FUNCTION_TEST_PARAM(XML_NODE, xml);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(callbackData != NULL);
    (void)name;
    (void)xml;

    // Only delete files since paths don't really exist
    if (type == storageTypeFile)
    {
        StorageAzurePathRemoveData *data = (StorageAzurePathRemoveData *)callbackData;
        storageInterfaceRemoveP(this, strNewFmt("%s/%s", strPtr(data->path), strPtr(name)));
        //
        // // If there is something to delete then create the request
        // if (data->xml == NULL)
        // {
        //     MEM_CONTEXT_BEGIN(data->memContext)
        //     {
        //         data->xml = xmlDocumentNew(AZURE_XML_TAG_DELETE_STR);
        //         xmlNodeContentSet(xmlNodeAdd(xmlDocumentRoot(data->xml), AZURE_XML_TAG_QUIET_STR), TRUE_STR);
        //     }
        //     MEM_CONTEXT_END();
        // }
        //
        // // Add to delete list
        // xmlNodeContentSet(
        //     xmlNodeAdd(xmlNodeAdd(xmlDocumentRoot(data->xml), AZURE_XML_TAG_OBJECT_STR), AZURE_XML_TAG_KEY_STR),
        //     xmlNodeContent(xmlNodeChild(xml, AZURE_XML_TAG_KEY_STR, true)));
        // data->size++;
        //
        // // Delete list when it is full
        // if (data->size == this->deleteMax)
        // {
        //     storageAzurePathRemoveInternal(this, data->xml);
        //
        //     xmlDocumentFree(data->xml);
        //     data->xml = NULL;
        //     data->size = 0;
        // }
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
        StorageAzurePathRemoveData data = {.path = path};
        storageAzureListInternal(this, path, NULL, true, storageAzurePathRemoveCallback, &data);
        //
        // if (data.xml != NULL)
        //     storageAzurePathRemoveInternal(this, data.xml);
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

    storageAzureRequestP(this, HTTP_VERB_DELETE_STR, file, .returnContent = true, .allowMissing = true);

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
static const StorageInterface storageInterfaceAzure =
{
    .info = storageAzureInfo,
    .infoList = storageAzureInfoList,
    .newRead = storageAzureNewRead,
    .newWrite = storageAzureNewWrite,
    .pathRemove = storageAzurePathRemove,
    .remove = storageAzureRemove,
};

Storage *
storageAzureNew(
    const String *path, bool write, StoragePathExpressionCallback pathExpressionFunction, const String *container,
    const String *account, const String *key, size_t partSize, const String *host, unsigned int port, TimeMSec timeout,
    bool verifyPeer, const String *caFile, const String *caPath)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STRING, path);
        FUNCTION_LOG_PARAM(BOOL, write);
        FUNCTION_LOG_PARAM(FUNCTIONP, pathExpressionFunction);
        FUNCTION_LOG_PARAM(STRING, container);
        FUNCTION_LOG_PARAM(STRING, account);
        FUNCTION_LOG_PARAM(STRING, key);
        FUNCTION_LOG_PARAM(SIZE, partSize);
        FUNCTION_LOG_PARAM(STRING, host);
        FUNCTION_LOG_PARAM(UINT, port);
        FUNCTION_LOG_PARAM(TIME_MSEC, timeout);
        FUNCTION_LOG_PARAM(BOOL, verifyPeer);
        FUNCTION_LOG_PARAM(STRING, caFile);
        FUNCTION_LOG_PARAM(STRING, caPath);
    FUNCTION_LOG_END();

    ASSERT(path != NULL);

    Storage *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("StorageAzure")
    {
        StorageAzure *driver = memNew(sizeof(StorageAzure));

        *driver = (StorageAzure)
        {
            .memContext = MEM_CONTEXT_NEW(),
            .interface = storageInterfaceAzure,
            .container = strDup(container),
            .account = strDup(account),
            .key = strDup(key),
            .partSize = partSize,
            .host = host == NULL ? STRDEF("BOGUS") : host,
            .uriPrefix = host == NULL ?
                strNewFmt("/%s", strPtr(container)) : strNewFmt("/%s/%s", strPtr(account), strPtr(container)),
        };

        // Create the http client cache used to service requests
        driver->httpClientCache = httpClientCacheNew(driver->host, port, timeout, verifyPeer, caFile, caPath);

        // Create list of redacted headers
        driver->headerRedactList = strLstNew();
        strLstAdd(driver->headerRedactList, HTTP_HEADER_AUTHORIZATION_STR);

        this = storageNew(
            STORAGE_AZURE_TYPE_STR, path, 0, 0, write, pathExpressionFunction, driver, driver->interface);
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_LOG_RETURN(STORAGE, this);
}
