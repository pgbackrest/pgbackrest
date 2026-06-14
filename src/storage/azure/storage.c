/***********************************************************************************************************************************
Azure Storage
***********************************************************************************************************************************/
#include <build.h>

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
#include "common/stat.h"
#include "common/type/json.h"
#include "common/type/object.h"
#include "common/type/xml.h"
#include "storage/azure/read.h"
#include "storage/azure/write.h"

/***********************************************************************************************************************************
Defaults
***********************************************************************************************************************************/
#define STORAGE_AZURE_DELETE_MAX                                    256

/***********************************************************************************************************************************
Azure http headers
***********************************************************************************************************************************/
STRING_STATIC(AZURE_HEADER_TAGS,                                    "x-ms-tags");
STRING_STATIC(AZURE_HEADER_VERSION_STR,                             "x-ms-version");
STRING_STATIC(AZURE_HEADER_VERSION_VALUE_STR,                       "2024-08-04");

/***********************************************************************************************************************************
Azure query tokens
***********************************************************************************************************************************/
STRING_STATIC(AZURE_QUERY_MARKER_STR,                               "marker");
STRING_STATIC(AZURE_QUERY_BATCH_STR,                                "batch");
STRING_EXTERN(AZURE_QUERY_COMP_STR,                                 AZURE_QUERY_COMP);
STRING_STATIC(AZURE_QUERY_DELIMITER_STR,                            "delimiter");
STRING_STATIC(AZURE_QUERY_INCLUDE_STR,                              "include");
STRING_STATIC(AZURE_QUERY_PREFIX_STR,                               "prefix");
STRING_EXTERN(AZURE_QUERY_RESTYPE_STR,                              AZURE_QUERY_RESTYPE);
STRING_STATIC(AZURE_QUERY_SIG_STR,                                  "sig");

STRING_STATIC(AZURE_QUERY_VALUE_LIST_STR,                           "list");
STRING_EXTERN(AZURE_QUERY_VALUE_CONTAINER_STR,                      AZURE_QUERY_VALUE_CONTAINER);
STRING_STATIC(AZURE_QUERY_VALUE_VERSIONS_STR,                       "versions");
STRING_STATIC(AZURE_QUERY_API_VERSION,                              "api-version");
STRING_STATIC(AZURE_QUERY_RESOURCE,                                 "resource");

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
STRING_STATIC(AZURE_XML_TAG_VERSION_ID_STR,                         "VersionId");

/***********************************************************************************************************************************
Constants required for Azure managed identities

Documentation for the response format is found at:
https://learn.microsoft.com/en-us/entra/identity/managed-identities-azure-resources/how-to-use-vm-token#get-a-token-using-curl
***********************************************************************************************************************************/
STRING_STATIC(AZURE_CREDENTIAL_HOST_STR,                            "169.254.169.254");
#define AZURE_CREDENTIAL_PORT                                       80
#define AZURE_CREDENTIAL_PATH                                       "/metadata/identity/oauth2/token"
#define AZURE_CREDENTIAL_API_VERSION                                "2018-02-01"

VARIANT_STRDEF_STATIC(AZURE_JSON_TAG_ACCESS_TOKEN_VAR,              "access_token");
VARIANT_STRDEF_STATIC(AZURE_JSON_TAG_EXPIRES_IN_VAR,                "expires_in");

/***********************************************************************************************************************************
Statistics constants
***********************************************************************************************************************************/
STRING_STATIC(AZURE_STAT_REMOVE_STR,                                "azure.rm");
STRING_STATIC(AZURE_STAT_REMOVE_BATCH_STR,                          "azure.rm.batch");
STRING_STATIC(AZURE_STAT_REMOVE_BATCH_PART_STR,                     "azure.rm.batch.part");
STRING_STATIC(AZURE_STAT_REMOVE_BATCH_RETRY_STR,                    "azure.rm.batch.retry");

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct StorageAzure
{
    STORAGE_COMMON_MEMBER;
    HttpClient *httpClient;                                         // Http client to service requests
    StringList *headerRedactList;                                   // List of headers to redact from logging
    StringList *queryRedactList;                                    // List of query keys to redact from logging

    StorageAzureKeyType keyType;                                    // Key type (e.g. storageAzureKeyTypeShared)
    const String *container;                                        // Container to store data in
    const String *account;                                          // Account
    const Buffer *sharedKey;                                        // Shared key
    const HttpQuery *sasKey;                                        // SAS key
    const String *host;                                             // Host name
    size_t blockSize;                                               // Block size for multi-block upload
    unsigned int deleteMax;                                         // Maximum objects that can be deleted in one request
    const String *tag;                                              // Tags to be applied to objects
    const String *pathPrefix;                                       // Account/container prefix

    uint64_t fileId;                                                // Id to used to make file block identifiers unique

    // For Azure managed identities authentication
    HttpClient *credHttpClient;                                     // HTTP client to service credential requests
    const String *credHost;                                         // Credentials host
    String *accessToken;                                            // Access token
    time_t accessTokenExpirationTime;                               // Time the access token expires
};

/***********************************************************************************************************************************
Generate authorization header and add it to the supplied header list

Based on the documentation at https://docs.microsoft.com/en-us/rest/api/storageservices/authorize-with-shared-key
***********************************************************************************************************************************/
static void
storageAzureAuth(
    StorageAzure *const this, const String *const verb, const String *const path, HttpQuery *const query,
    const String *const dateTime, HttpHeader *const httpHeader, const bool subRequest)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_AZURE, this);
        FUNCTION_TEST_PARAM(STRING, verb);
        FUNCTION_TEST_PARAM(STRING, path);
        FUNCTION_TEST_PARAM(HTTP_QUERY, query);
        FUNCTION_TEST_PARAM(STRING, dateTime);
        FUNCTION_TEST_PARAM(KEY_VALUE, httpHeader);
        FUNCTION_TEST_PARAM(BOOL, subRequest);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(verb != NULL);
    ASSERT(path != NULL);
    ASSERT(dateTime != NULL);
    ASSERT(httpHeader != NULL);
    ASSERT(httpHeaderGet(httpHeader, HTTP_HEADER_CONTENT_LENGTH_STR) != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Host header is required for all authentication types but must not be set on batch sub-requests
        if (!subRequest)
            httpHeaderPut(httpHeader, HTTP_HEADER_HOST_STR, this->host);

        // Date header is required for shared key authentication (for signing)
        if (this->keyType == storageAzureKeyTypeShared)
            httpHeaderPut(httpHeader, HTTP_HEADER_DATE_STR, dateTime);

        // Set version header (required for shared key and auto auth types, not for SAS) but never on batch sub-requests,
        // which inherit the version from the top-level batch request
        if (this->keyType != storageAzureKeyTypeSas && !subRequest)
            httpHeaderPut(httpHeader, AZURE_HEADER_VERSION_STR, AZURE_HEADER_VERSION_VALUE_STR);

        // Shared key authentication
        if (this->keyType == storageAzureKeyTypeShared)
        {
            ASSERT(this->sharedKey != NULL);

            // Generate canonical headers
            String *const headerCanonical = strNew();
            const StringList *const headerKeyList = httpHeaderList(httpHeader);

            for (unsigned int headerKeyIdx = 0; headerKeyIdx < strLstSize(headerKeyList); headerKeyIdx++)
            {
                const String *const headerKey = strLstGet(headerKeyList, headerKeyIdx);

                if (!strBeginsWithZ(headerKey, "x-ms-"))
                    continue;

                strCatFmt(headerCanonical, "%s:%s\n", strZ(headerKey), strZ(httpHeaderGet(httpHeader, headerKey)));
            }

            // Generate canonical query
            String *const queryCanonical = strNew();

            if (query != NULL)
            {
                const StringList *const queryKeyList = httpQueryList(query);
                ASSERT(!strLstEmpty(queryKeyList));

                for (unsigned int queryKeyIdx = 0; queryKeyIdx < strLstSize(queryKeyList); queryKeyIdx++)
                {
                    const String *const queryKey = strLstGet(queryKeyList, queryKeyIdx);

                    strCatFmt(queryCanonical, "\n%s:%s", strZ(queryKey), strZ(httpQueryGet(query, queryKey)));
                }
            }

            // Generate string to sign
            const String *const contentLength = httpHeaderGet(httpHeader, HTTP_HEADER_CONTENT_LENGTH_STR);
            const String *const contentMd5 = httpHeaderGet(httpHeader, HTTP_HEADER_CONTENT_MD5_STR);
            const String *const contentType = httpHeaderGet(httpHeader, HTTP_HEADER_CONTENT_TYPE_STR);
            const String *const range = httpHeaderGet(httpHeader, HTTP_HEADER_RANGE_STR);

            const String *const stringToSign = strNewFmt(
                "%s\n"                                                  // verb
                "\n"                                                    // content-encoding
                "\n"                                                    // content-language
                "%s\n"                                                  // content-length
                "%s\n"                                                  // content-md5
                "%s\n"                                                  // content-type
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
                contentType == NULL ? "" : strZ(contentType), strZ(dateTime), range == NULL ? "" : strZ(range),
                strZ(headerCanonical), strZ(this->account), strZ(path), strZ(queryCanonical));

            // Generate authorization header
            httpHeaderPut(
                httpHeader, HTTP_HEADER_AUTHORIZATION_STR,
                strNewFmt(
                    "SharedKey %s:%s", strZ(this->account),
                    strZ(strNewEncode(encodingBase64, cryptoHmacOne(hashTypeSha256, this->sharedKey, BUFSTR(stringToSign))))));
        }
        // Auto authentication
        else if (this->keyType == storageAzureKeyTypeAuto)
        {
            const time_t timeBegin = time(NULL);

            if (timeBegin >= this->accessTokenExpirationTime)
            {
                // Retrieve the access token via the Managed Identities endpoint
                HttpHeader *const authHeader = httpHeaderNew(NULL);
                httpHeaderAdd(authHeader, STRDEF("Metadata"), TRUE_STR);
                httpHeaderAdd(authHeader, HTTP_HEADER_HOST_STR, this->credHost);
                httpHeaderAdd(authHeader, HTTP_HEADER_CONTENT_LENGTH_STR, ZERO_STR);

                HttpQuery *const authQuery = httpQueryNewP();
                httpQueryAdd(authQuery, AZURE_QUERY_API_VERSION, STRDEF(AZURE_CREDENTIAL_API_VERSION));
                httpQueryAdd(authQuery, AZURE_QUERY_RESOURCE, strNewFmt("https://%s", strZ(this->host)));

                HttpRequest *const request = httpRequestNewP(
                    this->credHttpClient, HTTP_VERB_GET_STR, STRDEF(AZURE_CREDENTIAL_PATH), .header = authHeader,
                    .query = authQuery);
                HttpResponse *const response = httpRequestResponse(request, true);

                // Set the access_token on success and store an expiration time when we should re-fetch it
                if (httpResponseCodeOk(response))
                {
                    // Get credentials and expiration from the JSON response
                    const KeyValue *const credential = varKv(jsonToVar(strNewBuf(httpResponseContent(response))));
                    const String *const accessToken = varStr(kvGet(credential, AZURE_JSON_TAG_ACCESS_TOKEN_VAR));
                    CHECK(FormatError, accessToken != NULL, "access token missing");

                    const Variant *const expiresInStr = kvGet(credential, AZURE_JSON_TAG_EXPIRES_IN_VAR);
                    CHECK(FormatError, expiresInStr != NULL, "expiry missing");

                    MEM_CONTEXT_OBJ_BEGIN(this)
                    {
                        strCat(strTrunc(this->accessToken), accessToken);

                        // Subtract http client timeout * 2 so the token does not expire in the middle of http retries
                        const time_t clientTimeoutPeriod = ((time_t)(httpClientTimeout(this->httpClient) / MSEC_PER_SEC * 2));
                        const time_t expiresIn = (time_t)varInt64Force(expiresInStr);

                        this->accessTokenExpirationTime = timeBegin + expiresIn - clientTimeoutPeriod;
                    }
                    MEM_CONTEXT_OBJ_END();
                }
                else
                    httpRequestError(request, response);
            }

            // Add authorization header
            httpHeaderPut(httpHeader, HTTP_HEADER_AUTHORIZATION_STR, strNewFmt("Bearer %s", strZ(this->accessToken)));
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
FN_EXTERN HttpRequest *
storageAzureRequestAsync(StorageAzure *const this, const String *const verb, StorageAzureRequestAsyncParam param)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STORAGE_AZURE, this);
        FUNCTION_LOG_PARAM(STRING, verb);
        FUNCTION_LOG_PARAM(STRING, param.path);
        FUNCTION_LOG_PARAM(HTTP_HEADER, param.header);
        FUNCTION_LOG_PARAM(HTTP_QUERY, param.query);
        FUNCTION_LOG_PARAM(BUFFER, param.content);
        FUNCTION_LOG_PARAM(LIST, param.contentList);
        FUNCTION_LOG_PARAM(BOOL, param.tag);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(verb != NULL);

    HttpRequest *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Prepend path prefix
        param.path =
            param.path == NULL ?
                this->pathPrefix :
                strNewFmt("%s%s", strZ(this->pathPrefix), strZ(param.path));

        // Create header list
        HttpHeader *requestHeader =
            param.header == NULL ? httpHeaderNew(this->headerRedactList) : httpHeaderDup(param.header, this->headerRedactList);

        // Set content or construct multipart content
        const String *const date = httpDateFromTime(time(NULL));
        const Buffer *content = param.content;

        if (param.contentList != NULL)
        {
            ASSERT(param.content == NULL);
            ASSERT(!lstEmpty(param.contentList));

            HttpRequestMulti *const requestMulti = httpRequestMultiNew();

            for (unsigned int contentIdx = 0; contentIdx < lstSize(param.contentList); contentIdx++)
            {
                const StorageAzureRequestPart *const requestPart = lstGet(param.contentList, contentIdx);
                HttpHeader *const partHeader = httpHeaderNew(this->headerRedactList);
                HttpQuery *const partQuery = this->sasKey != NULL ? httpQueryNewP(.redactList = this->queryRedactList) : NULL;
                const String *const path = strNewFmt("%s%s", strZ(this->pathPrefix), strZ(requestPart->path));

                httpHeaderAdd(partHeader, HTTP_HEADER_CONTENT_LENGTH_STR, ZERO_STR);
                storageAzureAuth(this, requestPart->verb, path, partQuery, date, partHeader, true);

                httpRequestMultiAddP(
                    requestMulti, strNewFmt("%u", contentIdx), requestPart->verb, path, .header = partHeader, .query = partQuery);
            }

            httpRequestMultiHeaderAdd(requestMulti, requestHeader);
            content = httpRequestMultiContent(requestMulti);
        }

        // Set content length
        httpHeaderAdd(
            requestHeader, HTTP_HEADER_CONTENT_LENGTH_STR,
            content == NULL || bufEmpty(content) ? ZERO_STR : strNewFmt("%zu", bufUsed(content)));

        // Calculate content-md5 header if there is content
        if (content != NULL)
        {
            httpHeaderAdd(
                requestHeader, HTTP_HEADER_CONTENT_MD5_STR, strNewEncode(encodingBase64, cryptoHashOne(hashTypeMd5, content)));
        }

        // Set tags when requested and available
        if (param.tag && this->tag != NULL)
            httpHeaderPut(requestHeader, AZURE_HEADER_TAGS, this->tag);

        // Encode path
        const String *const path = httpUriEncode(param.path, true);

        // Make a copy of the query so it can be modified
        HttpQuery *const query =
            this->sasKey != NULL && param.query == NULL ?
                httpQueryNewP(.redactList = this->queryRedactList) :
                httpQueryDupP(param.query, .redactList = this->queryRedactList);

        // Generate authorization header
        storageAzureAuth(this, verb, path, query, date, requestHeader, false);

        // Send request
        MEM_CONTEXT_PRIOR_BEGIN()
        {
            result = httpRequestNewP(
                this->httpClient, verb, path, .query = query, .header = requestHeader, .content = content);
        }
        MEM_CONTEXT_END();
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(HTTP_REQUEST, result);
}

FN_EXTERN HttpResponse *
storageAzureResponse(HttpRequest *const request, const StorageAzureResponseParam param)
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

FN_EXTERN HttpResponse *
storageAzureRequest(StorageAzure *const this, const String *const verb, const StorageAzureRequestParam param)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STORAGE_AZURE, this);
        FUNCTION_LOG_PARAM(STRING, verb);
        FUNCTION_LOG_PARAM(STRING, param.path);
        FUNCTION_LOG_PARAM(HTTP_HEADER, param.header);
        FUNCTION_LOG_PARAM(HTTP_QUERY, param.query);
        FUNCTION_LOG_PARAM(BUFFER, param.content);
        FUNCTION_LOG_PARAM(LIST, param.contentList);
        FUNCTION_LOG_PARAM(BOOL, param.allowMissing);
        FUNCTION_LOG_PARAM(BOOL, param.contentIo);
        FUNCTION_LOG_PARAM(BOOL, param.tag);
    FUNCTION_LOG_END();

    HttpRequest *const request = storageAzureRequestAsyncP(
        this, verb, .path = param.path, .header = param.header, .query = param.query, .content = param.content,
        .contentList = param.contentList, .tag = param.tag);
    HttpResponse *const result = storageAzureResponseP(request, .allowMissing = param.allowMissing, .contentIo = param.contentIo);

    httpRequestFree(request);

    FUNCTION_LOG_RETURN(HTTP_RESPONSE, result);
}

/***********************************************************************************************************************************
General function for listing files to be used by other list routines

Based on the documentation at https://learn.microsoft.com/en-us/rest/api/storageservices/list-blobs
***********************************************************************************************************************************/
static void
storageAzureListInternal(
    StorageAzure *const this, const String *const path, const StorageInfoLevel level, const String *const expression,
    const bool recurse, const time_t targetTime, const StorageListCallback callback, void *const callbackData)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STORAGE_AZURE, this);
        FUNCTION_LOG_PARAM(STRING, path);
        FUNCTION_LOG_PARAM(ENUM, level);
        FUNCTION_LOG_PARAM(STRING, expression);
        FUNCTION_LOG_PARAM(BOOL, recurse);
        FUNCTION_LOG_PARAM(TIME, targetTime);
        FUNCTION_LOG_PARAM(FUNCTIONP, callback);
        FUNCTION_LOG_PARAM_P(VOID, callbackData);
    FUNCTION_LOG_END();

    FUNCTION_AUDIT_CALLBACK();

    ASSERT(this != NULL);
    ASSERT(path != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Build the base prefix by stripping off the initial /
        const String *const basePrefix = strSize(path) == 1 ? EMPTY_STR : strNewFmt("%s/", strZ(strSub(path, 1)));

        // Get the expression prefix when possible to limit initial results
        const String *const expressionPrefix = regExpPrefix(expression);

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
        HttpQuery *const query = httpQueryNewP();

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

        // Add versions
        if (targetTime != 0)
            httpQueryAdd(query, AZURE_QUERY_INCLUDE_STR, AZURE_QUERY_VALUE_VERSIONS_STR);

        // Store last info so it can be updated across requests for versioning
        String *const nameLast = strNew();
        String *const versionIdLast = strNew();
        StorageInfo infoLast = {.level = level, .name = nameLast};

        if (targetTime != 0)
            infoLast.versionId = versionIdLast;

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

                const XmlNode *const xmlRoot = xmlDocumentRoot(xmlDocumentNewBuf(httpResponseContent(response)));

                // If a continuation marker exists then send an async request to get more data
                const String *const continuationMarker = xmlNodeContent(
                    xmlNodeChild(xmlRoot, AZURE_XML_TAG_NEXT_MARKER_STR, false));

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
                const XmlNode *const blobs = xmlNodeChild(xmlRoot, AZURE_XML_TAG_BLOBS_STR, true);
                const XmlNodeList *const blobPrefixList = xmlNodeChildList(blobs, AZURE_XML_TAG_BLOB_PREFIX_STR);

                for (unsigned int blobPrefixIdx = 0; blobPrefixIdx < xmlNodeLstSize(blobPrefixList); blobPrefixIdx++)
                {
                    const XmlNode *const subPathNode = xmlNodeLstGet(blobPrefixList, blobPrefixIdx);

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
                const XmlNodeList *const fileList = xmlNodeChildList(blobs, AZURE_XML_TAG_BLOB_STR);

                for (unsigned int fileIdx = 0; fileIdx < xmlNodeLstSize(fileList); fileIdx++)
                {
                    const XmlNode *const fileNode = xmlNodeLstGet(fileList, fileIdx);
                    const XmlNode *const property = xmlNodeChild(fileNode, AZURE_XML_TAG_PROPERTIES_STR, true);

                    // Get file name and strip off the base prefix when present
                    const String *name = xmlNodeContent(xmlNodeChild(fileNode, AZURE_XML_TAG_NAME_STR, true));

                    if (!strEmpty(basePrefix))
                        name = strSub(name, strSize(basePrefix));

                    // Return info for last file if new file
                    if (infoLast.exists && !strEq(name, nameLast))
                    {
                        callback(callbackData, &infoLast);
                        infoLast.exists = false;
                    }

                    // If targeting by time exclude versions that are newer than targetTime. Note that the API does not provide
                    // reliable delete markers so the filtering will also show files that have been deleted rather than replaced
                    // with a new version. The problem with the delete markers is that Creation-Time/Last-Modified are set equal to
                    // the times in the last version so we don't know when the file was deleted. It might be possible to use
                    // VersionId for this purpose, since it appears to be a timestamp, but the field is described as "opaque" in the
                    // documentation so it does not seem to be a good idea to use it.
                    if (targetTime != 0)
                    {
                        infoLast.timeModified = httpDateToTime(
                            xmlNodeContent(xmlNodeChild(property, AZURE_XML_TAG_LAST_MODIFIED_STR, true)));

                        // Skip this version if it is newer than the time limit
                        if (infoLast.timeModified > targetTime)
                            continue;
                    }

                    // Update last name and set exists
                    strCat(strTrunc(nameLast), name);
                    infoLast.exists = true;

                    // Add basic info if requested (no need to add type info since file is default type)
                    if (level >= storageInfoLevelBasic)
                    {
                        infoLast.size = cvtZToUInt64(
                            strZ(xmlNodeContent(xmlNodeChild(property, AZURE_XML_TAG_CONTENT_LENGTH_STR, true))));

                        if (targetTime == 0)
                        {
                            infoLast.timeModified = httpDateToTime(
                                xmlNodeContent(xmlNodeChild(property, AZURE_XML_TAG_LAST_MODIFIED_STR, true)));
                        }
                        else
                        {
                            strCat(
                                strTrunc(versionIdLast),
                                xmlNodeContent(xmlNodeChild(fileNode, AZURE_XML_TAG_VERSION_ID_STR, true)));
                        }
                    }
                }
            }
            MEM_CONTEXT_TEMP_END();
        }
        while (request != NULL);

        // Callback with last info if it exists
        if (infoLast.exists)
            callback(callbackData, &infoLast);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
static StorageInfo
storageAzureInfo(THIS_VOID, const String *const file, const StorageInfoLevel level, const StorageInterfaceInfoParam param)
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
        FUNCTION_LOG_PARAM(TIME, param.targetTime);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(path != NULL);

    StorageList *const result = storageLstNew(level);

    storageAzureListInternal(this, path, level, param.expression, false, param.targetTime, storageAzureListCallback, result);

    FUNCTION_LOG_RETURN(STORAGE_LIST, result);
}

/**********************************************************************************************************************************/
static void *
storageAzureNewRead(THIS_VOID, const String *const file, const StorageInterfaceNewReadParam param)
{
    THIS(StorageAzure);

    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STORAGE_AZURE, this);
        FUNCTION_LOG_PARAM(STRING, file);
        FUNCTION_LOG_PARAM(UINT64, param.offset);
        FUNCTION_LOG_PARAM(VARIANT, param.limit);
        FUNCTION_LOG_PARAM(STRING, param.versionId);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(file != NULL);

    FUNCTION_LOG_RETURN(STORAGE_READ_AZURE, storageReadAzureNew(this, file, param.offset, param.limit, param.versionId));
}

/**********************************************************************************************************************************/
static void *
storageAzureNewWrite(THIS_VOID, const String *const file, const StorageInterfaceNewWriteParam param)
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
    ASSERT(param.truncate);
    ASSERT(param.user == NULL);
    ASSERT(param.group == NULL);
    ASSERT(param.timeModified == 0);

    FUNCTION_LOG_RETURN(STORAGE_WRITE_AZURE, storageWriteAzureNew(this, file, this->fileId++, this->blockSize));
}

/**********************************************************************************************************************************/
typedef struct StorageAzurePathRemoveData
{
    StorageAzure *this;                                             // Storage object
    MemContext *memContext;                                         // Mem context to create requests in
    HttpRequest *request;                                           // Async remove request
    List *requestContentList;                                       // Content list for async request
    List *contentList;                                              // Content list currently being built
    const String *path;                                             // Root path of remove
} StorageAzurePathRemoveData;

static void
storageAzurePathRemoveInternal(StorageAzurePathRemoveData *const data)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM_P(VOID, data);
    FUNCTION_TEST_END();

    ASSERT(data != NULL);
    ASSERT(data->this != NULL);

    // Get response for async request
    if (data->request != NULL)
    {
        MEM_CONTEXT_TEMP_BEGIN()
        {
            HttpResponse *const response = storageAzureResponseP(data->request);

            HttpResponseMulti *const responseMulti = httpResponseMultiNew(
                httpResponseContent(response), httpHeaderGet(httpResponseHeader(response), HTTP_HEADER_CONTENT_TYPE_STR));

            // Loop through all response parts. Parts are mapped to the original request by ordinal position since the service
            // returns one response part per sub-request in request order. The echoed content-id is intentionally not used because
            // Azure omits it on some error responses, which would otherwise make a failed part impossible to retry.
            unsigned int partIdx = 0;
            HttpResponse *responsePart = httpResponseMultiNext(responseMulti);
            CHECK(FormatError, responsePart != NULL, "at least one response part is required");

            do
            {
                // If not OK and not missing then retry
                if (!httpResponseCodeOk(responsePart) && httpResponseCode(responsePart) != HTTP_RESPONSE_CODE_NOT_FOUND)
                {
                    // Get the original request for this part by position
                    CHECK_FMT(
                        FormatError, partIdx < lstSize(data->requestContentList), "response part %u is out of range", partIdx);
                    const StorageAzureRequestPart *const content = lstGet(data->requestContentList, partIdx);

                    // Retry remove
                    statInc(AZURE_STAT_REMOVE_BATCH_RETRY_STR);
                    httpResponseFree(storageAzureRequestP(data->this, content->verb, .path = content->path, .allowMissing = true));
                }
                else
                    statInc(AZURE_STAT_REMOVE_BATCH_PART_STR);

                httpResponseFree(responsePart);
                responsePart = httpResponseMultiNext(responseMulti);
                partIdx++;
            }
            while (responsePart != NULL);
        }
        MEM_CONTEXT_TEMP_END();

        // Free request
        httpRequestFree(data->request);
        data->request = NULL;

        // Free content list
        lstFree(data->requestContentList);
    }

    // Send new async request if there is more to remove
    if (data->contentList != NULL)
    {
        statInc(AZURE_STAT_REMOVE_BATCH_STR);

        MEM_CONTEXT_BEGIN(data->memContext)
        {
            data->request = storageAzureRequestAsyncP(
                data->this, HTTP_VERB_POST_STR,
                .query = httpQueryAdd(
                    httpQueryAdd(httpQueryNewP(), AZURE_QUERY_COMP_STR, AZURE_QUERY_BATCH_STR),
                    AZURE_QUERY_RESTYPE_STR, AZURE_QUERY_VALUE_CONTAINER_STR),
                .contentList = data->contentList);
        }
        MEM_CONTEXT_END();

        // Store the content list for use in error handling
        data->requestContentList = data->contentList;
        data->contentList = NULL;
    }

    FUNCTION_TEST_RETURN_VOID();
}

static void
storageAzurePathRemoveCallback(void *const callbackData, const StorageInfo *const info)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM_P(VOID, callbackData);
        FUNCTION_TEST_PARAM(STORAGE_INFO, info);
    FUNCTION_TEST_END();

    ASSERT(callbackData != NULL);
    ASSERT(info != NULL);

    // Only delete files since paths don't really exist
    if (info->type == storageTypeFile)
    {
        StorageAzurePathRemoveData *const data = callbackData;

        if (data->contentList == NULL)
        {
            MEM_CONTEXT_BEGIN(data->memContext)
            {
                data->contentList = lstNewP(sizeof(StorageAzureRequestPart));
            }
            MEM_CONTEXT_END();
        }

        MEM_CONTEXT_OBJ_BEGIN(data->contentList)
        {
            const StorageAzureRequestPart content =
            {
                .verb = HTTP_VERB_DELETE_STR,
                .path = strNewFmt("%s/%s", strZ(data->path), strZ(info->name)),
            };

            lstAdd(data->contentList, &content);
        }
        MEM_CONTEXT_OBJ_END();

        if (lstSize(data->contentList) == data->this->deleteMax)
            storageAzurePathRemoveInternal(data);
    }

    FUNCTION_TEST_RETURN_VOID();
}

static bool
storageAzurePathRemove(THIS_VOID, const String *const path, const bool recurse, const StorageInterfacePathRemoveParam param)
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

        MEM_CONTEXT_TEMP_BEGIN()
        {
            storageAzureListInternal(this, path, storageInfoLevelType, NULL, true, 0, storageAzurePathRemoveCallback, &data);

            // Call if there is more to be removed
            if (data.contentList != NULL)
                storageAzurePathRemoveInternal(&data);

            // Check response on last async request
            storageAzurePathRemoveInternal(&data);
        }
        MEM_CONTEXT_TEMP_END();
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(BOOL, true);
}

/**********************************************************************************************************************************/
static void
storageAzureRemove(THIS_VOID, const String *const file, const StorageInterfaceRemoveParam param)
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

    statInc(AZURE_STAT_REMOVE_STR);
    httpResponseFree(storageAzureRequestP(this, HTTP_VERB_DELETE_STR, file, .allowMissing = true));

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
static const StorageInterface storageInterfaceAzure =
{
    .feature = 1 << storageFeatureVersioning | 1 << storageFeatureReadRetry,

    .info = storageAzureInfo,
    .list = storageAzureList,
    .newRead = storageAzureNewRead,
    .newWrite = storageAzureNewWrite,
    .pathRemove = storageAzurePathRemove,
    .remove = storageAzureRemove,
};

FN_EXTERN Storage *
storageAzureNew(
    const String *const path, const bool write, const time_t targetTime, StoragePathExpressionCallback pathExpressionFunction,
    const String *const container, const String *const account, const StorageAzureKeyType keyType, const String *const key,
    const size_t blockSize, const KeyValue *const tag, const String *const endpoint, const StorageAzureUriStyle uriStyle,
    const unsigned int port, const TimeMSec timeout, const HttpProtocolType protocolType, const bool verifyPeer,
    const String *const caFile, const String *const caPath)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STRING, path);
        FUNCTION_LOG_PARAM(BOOL, write);
        FUNCTION_LOG_PARAM(TIME, targetTime);
        FUNCTION_LOG_PARAM(FUNCTIONP, pathExpressionFunction);
        FUNCTION_LOG_PARAM(STRING, container);
        FUNCTION_TEST_PARAM(STRING, account);
        FUNCTION_LOG_PARAM(STRING_ID, keyType);
        FUNCTION_TEST_PARAM(STRING, key);
        FUNCTION_LOG_PARAM(SIZE, blockSize);
        FUNCTION_LOG_PARAM(KEY_VALUE, tag);
        FUNCTION_LOG_PARAM(STRING, endpoint);
        FUNCTION_LOG_PARAM(ENUM, uriStyle);
        FUNCTION_LOG_PARAM(UINT, port);
        FUNCTION_LOG_PARAM(TIME_MSEC, timeout);
        FUNCTION_LOG_PARAM(ENUM, protocolType);
        FUNCTION_LOG_PARAM(BOOL, verifyPeer);
        FUNCTION_LOG_PARAM(STRING, caFile);
        FUNCTION_LOG_PARAM(STRING, caPath);
    FUNCTION_LOG_END();

    ASSERT(path != NULL);
    ASSERT(container != NULL);
    ASSERT(account != NULL);
    ASSERT(endpoint != NULL);
    ASSERT(keyType == storageAzureKeyTypeAuto || key != NULL);
    ASSERT(blockSize != 0);

    OBJ_NEW_BEGIN(StorageAzure, .childQty = MEM_CONTEXT_QTY_MAX)
    {
        *this = (StorageAzure)
        {
            .interface = storageInterfaceAzure,
            .container = strDup(container),
            .account = strDup(account),
            .blockSize = blockSize,
            .host = uriStyle == storageAzureUriStyleHost ? strNewFmt("%s.%s", strZ(account), strZ(endpoint)) : strDup(endpoint),
            .pathPrefix =
                uriStyle == storageAzureUriStyleHost ?
                    strNewFmt("/%s", strZ(container)) : strNewFmt("/%s/%s", strZ(account), strZ(container)),
            .deleteMax = STORAGE_AZURE_DELETE_MAX,
            .keyType = keyType,
        };

        // Create tag query string
        if (tag != NULL)
        {
            HttpQuery *const query = httpQueryNewP(.kv = tag);
            this->tag = httpQueryRenderP(query);
            httpQueryFree(query);
        }

        // Initialization by key type
        switch (keyType)
        {
            // Create authentication client
            case storageAzureKeyTypeAuto:
                this->accessToken = strNew();
                this->credHost = AZURE_CREDENTIAL_HOST_STR;
                this->credHttpClient = httpClientNew(
                    sckClientNew(this->credHost, AZURE_CREDENTIAL_PORT, timeout, timeout), timeout);
                break;

            // Store shared key
            case storageAzureKeyTypeShared:
                this->sharedKey = bufNewDecode(encodingBase64, key);
                break;

            // Parse sas query
            case storageAzureKeyTypeSas:
                this->sasKey = httpQueryNewStr(key);
                break;
        }

        // Create the http client used to service requests. Use plain socket for HTTP, TLS for HTTPS.
        IoClient *ioClient;

        if (protocolType == httpProtocolTypeHttp)
            ioClient = sckClientNew(this->host, port, timeout, timeout);
        else
        {
            ioClient = tlsClientNewP(
                sckClientNew(this->host, port, timeout, timeout), this->host, timeout, timeout, verifyPeer, .caFile = caFile,
                .caPath = caPath);
        }

        this->httpClient = httpClientNew(ioClient, timeout);

        // Create list of redacted headers
        this->headerRedactList = strLstNew();
        strLstAdd(this->headerRedactList, HTTP_HEADER_AUTHORIZATION_STR);
        strLstAdd(this->headerRedactList, HTTP_HEADER_DATE_STR);

        // Create list of redacted query keys
        this->queryRedactList = strLstNew();
        strLstAdd(this->queryRedactList, AZURE_QUERY_SIG_STR);

        // Generate starting file id
        cryptoRandomBytes((uint8_t *)&this->fileId, sizeof(this->fileId));
    }
    OBJ_NEW_END();

    FUNCTION_LOG_RETURN(
        STORAGE, storageNew(STORAGE_AZURE_TYPE, path, 0, 0, write, targetTime, pathExpressionFunction, this, this->interface));
}
