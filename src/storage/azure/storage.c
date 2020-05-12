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
STRING_STATIC(AZURE_HEADER_AUTHORIZATION_STR,                       "authorization");
// STRING_STATIC(AZURE_HEADER_HOST_STR,                                "host");
// STRING_STATIC(AZURE_HEADER_CONTENT_SHA256_STR,                      "x-amz-content-sha256");
// STRING_STATIC(AZURE_HEADER_DATE_STR,                                "x-amz-date");
// STRING_STATIC(AZURE_HEADER_TOKEN_STR,                               "x-amz-security-token");

/***********************************************************************************************************************************
Azure query tokens
***********************************************************************************************************************************/
// STRING_STATIC(AZURE_QUERY_CONTINUATION_TOKEN_STR,                   "continuation-token");
// STRING_STATIC(AZURE_QUERY_DELETE_STR,                               "delete");
// STRING_STATIC(AZURE_QUERY_DELIMITER_STR,                            "delimiter");
// STRING_STATIC(AZURE_QUERY_LIST_TYPE_STR,                            "list-type");
// STRING_STATIC(AZURE_QUERY_PREFIX_STR,                               "prefix");
//
// STRING_STATIC(AZURE_QUERY_VALUE_LIST_TYPE_2_STR,                    "2");

/***********************************************************************************************************************************
Azure errors
***********************************************************************************************************************************/
// STRING_STATIC(AZURE_ERROR_REQUEST_TIME_TOO_SKEWED_STR,              "RequestTimeTooSkewed");

/***********************************************************************************************************************************
XML tags
***********************************************************************************************************************************/
// STRING_STATIC(AZURE_XML_TAG_CODE_STR,                               "Code");
// STRING_STATIC(AZURE_XML_TAG_COMMON_PREFIXES_STR,                    "CommonPrefixes");
// STRING_STATIC(AZURE_XML_TAG_CONTENTS_STR,                           "Contents");
// STRING_STATIC(AZURE_XML_TAG_DELETE_STR,                             "Delete");
// STRING_STATIC(AZURE_XML_TAG_ERROR_STR,                              "Error");
// STRING_STATIC(AZURE_XML_TAG_KEY_STR,                                "Key");
// STRING_STATIC(AZURE_XML_TAG_LAST_MODIFIED_STR,                      "LastModified");
// STRING_STATIC(AZURE_XML_TAG_MESSAGE_STR,                            "Message");
// STRING_STATIC(AZURE_XML_TAG_NEXT_CONTINUATION_TOKEN_STR,            "NextContinuationToken");
// STRING_STATIC(AZURE_XML_TAG_OBJECT_STR,                             "Object");
// STRING_STATIC(AZURE_XML_TAG_PREFIX_STR,                             "Prefix");
// STRING_STATIC(AZURE_XML_TAG_QUIET_STR,                              "Quiet");
// STRING_STATIC(AZURE_XML_TAG_SIZE_STR,                               "Size");

/***********************************************************************************************************************************
AWS authentication v4 constants
***********************************************************************************************************************************/
// #define AZURE                                                       "azure"
//     BUFFER_STRDEF_STATIC(AZURE_BUF,                                 AZURE);
// #define AWS4                                                        "AWS4"
// #define AWS4_REQUEST                                                "aws4_request"
//     BUFFER_STRDEF_STATIC(AWS4_REQUEST_BUF,                          AWS4_REQUEST);
// #define AWS4_HMAC_SHA256                                            "AWS4-HMAC-SHA256"

/***********************************************************************************************************************************
Starting date for signing string so it will be regenerated on the first request
***********************************************************************************************************************************/
// STRING_STATIC(YYYYMMDD_STR,                                         "YYYYMMDD");

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
    size_t partSize;                                                // Part size for multi-part upload
    bool pathStyle;                                                 // Add container to the path (only for emulation)

    // Current signing key and date it is valid for
    // const String *signingKeyDate;                                   // Date of cached signing key (so we know when to regenerate)
    // const Buffer *signingKey;                                       // Cached signing key
};

/***********************************************************************************************************************************
Expected ISO-8601 data/time size
***********************************************************************************************************************************/
#define ISO_8601_DATE_TIME_SIZE                                     16

/***********************************************************************************************************************************
Format ISO-8601 date/time for authentication
***********************************************************************************************************************************/
// static String *
// storageAzureDateTime(time_t authTime)
// {
//     FUNCTION_TEST_BEGIN();
//         FUNCTION_TEST_PARAM(TIME, authTime);
//     FUNCTION_TEST_END();
//
//     char buffer[ISO_8601_DATE_TIME_SIZE + 1];
//
//     THROW_ON_SYS_ERROR(
//         strftime(buffer, sizeof(buffer), "%Y%m%dT%H%M%SZ", gmtime(&authTime)) != ISO_8601_DATE_TIME_SIZE, AssertError,
//         "unable to format date");
//
//     FUNCTION_TEST_RETURN(strNew(buffer));
// }

/***********************************************************************************************************************************
Generate authorization header and add it to the supplied header list

Based on the excellent documentation at !!!
***********************************************************************************************************************************/
// static void
// storageAzureAuth(
//     StorageAzure *this, const String *verb, const String *uri, const HttpQuery *query, const String *dateTime,
//     HttpHeader *httpHeader, const String *payloadHash)
// {
//     FUNCTION_TEST_BEGIN();
//         FUNCTION_TEST_PARAM(STORAGE_AZURE, this);
//         FUNCTION_TEST_PARAM(STRING, verb);
//         FUNCTION_TEST_PARAM(STRING, uri);
//         FUNCTION_TEST_PARAM(HTTP_QUERY, query);
//         FUNCTION_TEST_PARAM(STRING, dateTime);
//         FUNCTION_TEST_PARAM(KEY_VALUE, httpHeader);
//         FUNCTION_TEST_PARAM(STRING, payloadHash);
//     FUNCTION_TEST_END();
//
//     ASSERT(this != NULL);
//     ASSERT(verb != NULL);
//     ASSERT(uri != NULL);
//     ASSERT(dateTime != NULL);
//     ASSERT(httpHeader != NULL);
//     ASSERT(payloadHash != NULL);
//
//     MEM_CONTEXT_TEMP_BEGIN()
//     {
//         // Get date from datetime
//         const String *date = strSubN(dateTime, 0, 8);
//
//         // Set required headers
//         httpHeaderPut(httpHeader, AZURE_HEADER_CONTENT_SHA256_STR, payloadHash);
//         httpHeaderPut(httpHeader, AZURE_HEADER_DATE_STR, dateTime);
//         httpHeaderPut(httpHeader, AZURE_HEADER_HOST_STR, this->bucketEndpoint);
//
//         if (this->securityToken != NULL)
//             httpHeaderPut(httpHeader, AZURE_HEADER_TOKEN_STR, this->securityToken);
//
//         // Generate canonical request and signed headers
//         const StringList *headerList = strLstSort(strLstDup(httpHeaderList(httpHeader)), sortOrderAsc);
//         String *signedHeaders = NULL;
//
//         String *canonicalRequest = strNewFmt(
//             "%s\n%s\n%s\n", strPtr(verb), strPtr(uri), query == NULL ? "" : strPtr(httpQueryRender(query)));
//
//         for (unsigned int headerIdx = 0; headerIdx < strLstSize(headerList); headerIdx++)
//         {
//             const String *headerKey = strLstGet(headerList, headerIdx);
//             const String *headerKeyLower = strLower(strDup(headerKey));
//
//             // Skip the authorization header -- if it exists this is a retry
//             if (strEq(headerKeyLower, AZURE_HEADER_AUTHORIZATION_STR))
//                 continue;
//
//             strCatFmt(canonicalRequest, "%s:%s\n", strPtr(headerKeyLower), strPtr(httpHeaderGet(httpHeader, headerKey)));
//
//             if (signedHeaders == NULL)
//                 signedHeaders = strDup(headerKeyLower);
//             else
//                 strCatFmt(signedHeaders, ";%s", strPtr(headerKeyLower));
//         }
//
//         strCatFmt(canonicalRequest, "\n%s\n%s", strPtr(signedHeaders), strPtr(payloadHash));
//
//         // Generate string to sign
//         const String *stringToSign = strNewFmt(
//             AWS4_HMAC_SHA256 "\n%s\n%s/%s/" AZURE "/" AWS4_REQUEST "\n%s", strPtr(dateTime), strPtr(date), strPtr(this->region),
//             strPtr(bufHex(cryptoHashOne(HASH_TYPE_SHA256_STR, BUFSTR(canonicalRequest)))));
//
//         // Generate signing key.  This key only needs to be regenerated every seven days but we'll do it once a day to keep the
//         // logic simple.  It's a relatively expensive operation so we'd rather not do it for every request.
//         // If the cached signing key has expired (or has none been generated) then regenerate it
//         if (!strEq(date, this->signingKeyDate))
//         {
//             const Buffer *dateKey = cryptoHmacOne(
//                 HASH_TYPE_SHA256_STR, BUFSTR(strNewFmt(AWS4 "%s", strPtr(this->secretAccessKey))), BUFSTR(date));
//             const Buffer *regionKey = cryptoHmacOne(HASH_TYPE_SHA256_STR, dateKey, BUFSTR(this->region));
//             const Buffer *serviceKey = cryptoHmacOne(HASH_TYPE_SHA256_STR, regionKey, AZURE_BUF);
//
//             // Switch to the object context so signing key and date are not lost
//             MEM_CONTEXT_BEGIN(this->memContext)
//             {
//                 this->signingKey = cryptoHmacOne(HASH_TYPE_SHA256_STR, serviceKey, AWS4_REQUEST_BUF);
//                 this->signingKeyDate = strDup(date);
//             }
//             MEM_CONTEXT_END();
//         }
//
//         // Generate authorization header
//         const String *authorization = strNewFmt(
//             AWS4_HMAC_SHA256 " Credential=%s/%s/%s/" AZURE "/" AWS4_REQUEST ",SignedHeaders=%s,Signature=%s",
//             strPtr(this->accessKey), strPtr(date), strPtr(this->region), strPtr(signedHeaders),
//             strPtr(bufHex(cryptoHmacOne(HASH_TYPE_SHA256_STR, this->signingKey, BUFSTR(stringToSign)))));
//
//         httpHeaderPut(httpHeader, AZURE_HEADER_AUTHORIZATION_STR, authorization);
//     }
//     MEM_CONTEXT_TEMP_END();
//
//     FUNCTION_TEST_RETURN_VOID();
// }

/***********************************************************************************************************************************
Process Azure request
***********************************************************************************************************************************/
StorageAzureRequestResult
storageAzureRequest(
    StorageAzure *this, const String *verb, const String *uri, const HttpQuery *query, const Buffer *body, bool returnContent,
    bool allowMissing)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STORAGE_AZURE, this);
        FUNCTION_LOG_PARAM(STRING, verb);
        FUNCTION_LOG_PARAM(STRING, uri);
        FUNCTION_LOG_PARAM(HTTP_QUERY, query);
        FUNCTION_LOG_PARAM(BUFFER, body);
        FUNCTION_LOG_PARAM(BOOL, returnContent);
        FUNCTION_LOG_PARAM(BOOL, allowMissing);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(verb != NULL);
    ASSERT(uri != NULL);

    StorageAzureRequestResult result = {0};
    // unsigned int retryRemaining = 2;
    // bool done;
    //
    // do
    // {
    //     done = true;
    //
    //     MEM_CONTEXT_TEMP_BEGIN()
    //     {
    //         // Create header list and add content length
    //         HttpHeader *requestHeader = httpHeaderNew(this->headerRedactList);
    //
    //         // Set content length
    //         httpHeaderAdd(
    //             requestHeader, HTTP_HEADER_CONTENT_LENGTH_STR,
    //             body == NULL || bufUsed(body) == 0 ? ZERO_STR : strNewFmt("%zu", bufUsed(body)));
    //
    //         // Calculate content-md5 header if there is content
    //         if (body != NULL)
    //         {
    //             char md5Hash[HASH_TYPE_MD5_SIZE_HEX];
    //             encodeToStr(encodeBase64, bufPtr(cryptoHashOne(HASH_TYPE_MD5_STR, body)), HASH_TYPE_M5_SIZE, md5Hash);
    //             httpHeaderAdd(requestHeader, HTTP_HEADER_CONTENT_MD5_STR, STR(md5Hash));
    //         }
    //
    //         // Generate authorization header
    //         storageAzureAuth(
    //             this, verb, httpUriEncode(uri, true), query, storageAzureDateTime(time(NULL)), requestHeader,
    //             body == NULL || bufUsed(body) == 0 ? HASH_TYPE_SHA256_ZERO_STR : bufHex(cryptoHashOne(HASH_TYPE_SHA256_STR, body)));
    //
    //         // Get an http client
    //         HttpClient *httpClient = httpClientCacheGet(this->httpClientCache);
    //
    //         // Process request
    //         Buffer *response = httpClientRequest(httpClient, verb, uri, query, requestHeader, body, returnContent);
    //
    //         // Error if the request was not successful
    //         if (!httpClientResponseCodeOk(httpClient) &&
    //             (!allowMissing || httpClientResponseCode(httpClient) != HTTP_RESPONSE_CODE_NOT_FOUND))
    //         {
    //             // If there are retries remaining and a response parse it as XML to extract the Azure error code
    //             if (response != NULL && retryRemaining > 0)
    //             {
    //                 // Attempt to parse the XML and extract the Azure error code
    //                 TRY_BEGIN()
    //                 {
    //                     XmlNode *error = xmlDocumentRoot(xmlDocumentNewBuf(response));
    //                     const String *errorCode = xmlNodeContent(xmlNodeChild(error, AZURE_XML_TAG_CODE_STR, true));
    //
    //                     if (strEq(errorCode, AZURE_ERROR_REQUEST_TIME_TOO_SKEWED_STR))
    //                     {
    //                         LOG_DEBUG_FMT(
    //                             "retry %s: %s", strPtr(errorCode),
    //                             strPtr(xmlNodeContent(xmlNodeChild(error, AZURE_XML_TAG_MESSAGE_STR, true))));
    //
    //                         retryRemaining--;
    //                         done = false;
    //                     }
    //                 }
    //                 // On failure just drop through and report the error as usual
    //                 CATCH_ANY()
    //                 {
    //                 }
    //                 TRY_END();
    //             }
    //
    //             // If not done then retry instead of reporting the error
    //             if (done)
    //             {
    //                 // General error message
    //                 String *error = strNewFmt(
    //                     "Azure request failed with %u: %s", httpClientResponseCode(httpClient),
    //                     strPtr(httpClientResponseMessage(httpClient)));
    //
    //                 // Output uri/query
    //                 strCat(error, "\n*** URI/Query ***:");
    //
    //                 strCatFmt(error, "\n%s", strPtr(httpUriEncode(uri, true)));
    //
    //                 if (query != NULL)
    //                     strCatFmt(error, "?%s", strPtr(httpQueryRender(query)));
    //
    //                 // Output request headers
    //                 const StringList *requestHeaderList = httpHeaderList(requestHeader);
    //
    //                 strCat(error, "\n*** Request Headers ***:");
    //
    //                 for (unsigned int requestHeaderIdx = 0; requestHeaderIdx < strLstSize(requestHeaderList); requestHeaderIdx++)
    //                 {
    //                     const String *key = strLstGet(requestHeaderList, requestHeaderIdx);
    //
    //                     strCatFmt(
    //                         error, "\n%s: %s", strPtr(key),
    //                         httpHeaderRedact(requestHeader, key) || strEq(key, AZURE_HEADER_DATE_STR) ?
    //                             "<redacted>" : strPtr(httpHeaderGet(requestHeader, key)));
    //                 }
    //
    //                 // Output response headers
    //                 const HttpHeader *responseHeader = httpClientResponseHeader(httpClient);
    //                 const StringList *responseHeaderList = httpHeaderList(responseHeader);
    //
    //                 if (strLstSize(responseHeaderList) > 0)
    //                 {
    //                     strCat(error, "\n*** Response Headers ***:");
    //
    //                     for (unsigned int responseHeaderIdx = 0; responseHeaderIdx < strLstSize(responseHeaderList);
    //                             responseHeaderIdx++)
    //                     {
    //                         const String *key = strLstGet(responseHeaderList, responseHeaderIdx);
    //                         strCatFmt(error, "\n%s: %s", strPtr(key), strPtr(httpHeaderGet(responseHeader, key)));
    //                     }
    //                 }
    //
    //                 // If there was content then output it
    //                 if (response!= NULL)
    //                     strCatFmt(error, "\n*** Response Content ***:\n%s", strPtr(strNewBuf(response)));
    //
    //                 THROW(ProtocolError, strPtr(error));
    //             }
    //         }
    //         else
    //         {
    //             // On success move the buffer to the prior context
    //             result.httpClient = httpClient;
    //             result.responseHeader = httpHeaderMove(
    //                 httpHeaderDup(httpClientResponseHeader(httpClient), NULL), memContextPrior());
    //             result.response = bufMove(response, memContextPrior());
    //         }
    //
    //     }
    //     MEM_CONTEXT_TEMP_END();
    // }
    // while (!done);

    FUNCTION_LOG_RETURN(STORAGE_AZURE_REQUEST_RESULT, result);
}

/***********************************************************************************************************************************
General function for listing files to be used by other list routines
***********************************************************************************************************************************/
// static void
// storageAzureListInternal(
//     StorageAzure *this, const String *path, const String *expression, bool recurse,
//     void (*callback)(StorageAzure *this, void *callbackData, const String *name, StorageType type, const XmlNode *xml),
//     void *callbackData)
// {
//     FUNCTION_LOG_BEGIN(logLevelDebug);
//         FUNCTION_LOG_PARAM(STORAGE_AZURE, this);
//         FUNCTION_LOG_PARAM(STRING, path);
//         FUNCTION_LOG_PARAM(STRING, expression);
//         FUNCTION_LOG_PARAM(BOOL, recurse);
//         FUNCTION_LOG_PARAM(FUNCTIONP, callback);
//         FUNCTION_LOG_PARAM_P(VOID, callbackData);
//     FUNCTION_LOG_END();
//
//     ASSERT(this != NULL);
//     ASSERT(path != NULL);
//
//     MEM_CONTEXT_TEMP_BEGIN()
//     {
//         const String *continuationToken = NULL;
//
//         // Build the base prefix by stripping off the initial /
//         const String *basePrefix;
//
//         if (strSize(path) == 1)
//             basePrefix = EMPTY_STR;
//         else
//             basePrefix = strNewFmt("%s/", strPtr(strSub(path, 1)));
//
//         // Get the expression prefix when possible to limit initial results
//         const String *expressionPrefix = regExpPrefix(expression);
//
//         // If there is an expression prefix then use it to build the query prefix, otherwise query prefix is base prefix
//         const String *queryPrefix;
//
//         if (expressionPrefix == NULL)
//             queryPrefix = basePrefix;
//         else
//         {
//             if (strEmpty(basePrefix))
//                 queryPrefix = expressionPrefix;
//             else
//                 queryPrefix = strNewFmt("%s%s", strPtr(basePrefix), strPtr(expressionPrefix));
//         }
//
//         // Loop as long as a continuation token returned
//         do
//         {
//             // Use an inner mem context here because we could potentially be retrieving millions of files so it is a good idea to
//             // free memory at regular intervals
//             MEM_CONTEXT_TEMP_BEGIN()
//             {
//                 HttpQuery *query = httpQueryNew();
//
//                 // Add continuation token from the prior loop if any
//                 if (continuationToken != NULL)
//                     httpQueryAdd(query, AZURE_QUERY_CONTINUATION_TOKEN_STR, continuationToken);
//
//                 // Add the delimiter to not recurse
//                 if (!recurse)
//                     httpQueryAdd(query, AZURE_QUERY_DELIMITER_STR, FSLASH_STR);
//
//                 // Use list type 2
//                 httpQueryAdd(query, AZURE_QUERY_LIST_TYPE_STR, AZURE_QUERY_VALUE_LIST_TYPE_2_STR);
//
//                 // Don't specified empty prefix because it is the default
//                 if (!strEmpty(queryPrefix))
//                     httpQueryAdd(query, AZURE_QUERY_PREFIX_STR, queryPrefix);
//
//                 XmlNode *xmlRoot = xmlDocumentRoot(
//                     xmlDocumentNewBuf(
//                         storageAzureRequest(this, HTTP_VERB_GET_STR, FSLASH_STR, query, NULL, true, false).response));
//
//                 // Get subpath list
//                 XmlNodeList *subPathList = xmlNodeChildList(xmlRoot, AZURE_XML_TAG_COMMON_PREFIXES_STR);
//
//                 for (unsigned int subPathIdx = 0; subPathIdx < xmlNodeLstSize(subPathList); subPathIdx++)
//                 {
//                     const XmlNode *subPathNode = xmlNodeLstGet(subPathList, subPathIdx);
//
//                     // Get subpath name
//                     const String *subPath = xmlNodeContent(xmlNodeChild(subPathNode, AZURE_XML_TAG_PREFIX_STR, true));
//
//                     // Strip off base prefix and final /
//                     subPath = strSubN(subPath, strSize(basePrefix), strSize(subPath) - strSize(basePrefix) - 1);
//
//                     // Add to list
//                     callback(this, callbackData, subPath, storageTypePath, subPathNode);
//                 }
//
//                 // Get file list
//                 XmlNodeList *fileList = xmlNodeChildList(xmlRoot, AZURE_XML_TAG_CONTENTS_STR);
//
//                 for (unsigned int fileIdx = 0; fileIdx < xmlNodeLstSize(fileList); fileIdx++)
//                 {
//                     const XmlNode *fileNode = xmlNodeLstGet(fileList, fileIdx);
//
//                     // Get file name
//                     const String *file = xmlNodeContent(xmlNodeChild(fileNode, AZURE_XML_TAG_KEY_STR, true));
//
//                     // Strip off the base prefix when present
//                     file = strEmpty(basePrefix) ? file : strSub(file, strSize(basePrefix));
//
//                     // Add to list
//                     callback(this, callbackData, file, storageTypeFile, fileNode);
//                 }
//
//                 // Get the continuation token and store it in the outer temp context
//                 MEM_CONTEXT_PRIOR_BEGIN()
//                 {
//                     continuationToken = xmlNodeContent(xmlNodeChild(xmlRoot, AZURE_XML_TAG_NEXT_CONTINUATION_TOKEN_STR, false));
//                 }
//                 MEM_CONTEXT_PRIOR_END();
//             }
//             MEM_CONTEXT_TEMP_END();
//         }
//         while (continuationToken != NULL);
//     }
//     MEM_CONTEXT_TEMP_END();
//
//     FUNCTION_LOG_RETURN_VOID();
// }

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

    // // Attempt to get file info
    // StorageAzureRequestResult httpResult = storageAzureRequest(this, HTTP_VERB_HEAD_STR, file, NULL, NULL, true, true);
    //
    // // Does the file exist?
    // StorageInfo result = {.level = level, .exists = httpClientResponseCodeOk(httpResult.httpClient)};
    StorageInfo result = {.level = level};
    //
    // // Add basic level info if requested and the file exists
    // if (result.level >= storageInfoLevelBasic && result.exists)
    // {
    //     result.type = storageTypeFile;
    //     result.size = cvtZToUInt64(strPtr(httpHeaderGet(httpResult.responseHeader, HTTP_HEADER_CONTENT_LENGTH_STR)));
    //     result.timeModified = httpLastModifiedToTime(httpHeaderGet(httpResult.responseHeader, HTTP_HEADER_LAST_MODIFIED_STR));
    // }

    FUNCTION_LOG_RETURN(STORAGE_INFO, result);
}

/**********************************************************************************************************************************/
typedef struct StorageAzureInfoListData
{
    StorageInfoLevel level;                                         // Level of info to set
    StorageInfoListCallback callback;                               // User-supplied callback function
    void *callbackData;                                             // User-supplied callback data
} StorageAzureInfoListData;

// Helper to convert YYYY-MM-DDTHH:MM:SS.MSECZ format to time_t.  This format is very nearly ISO-8601 except for the inclusion of
// milliseconds which are discarded here.
// static time_t
// storageAzureCvtTime(const String *time)
// {
//     FUNCTION_TEST_BEGIN();
//         FUNCTION_TEST_PARAM(STRING, time);
//     FUNCTION_TEST_END();
//
//     FUNCTION_TEST_RETURN(
//         epochFromParts(
//             cvtZToInt(strPtr(strSubN(time, 0, 4))), cvtZToInt(strPtr(strSubN(time, 5, 2))),
//             cvtZToInt(strPtr(strSubN(time, 8, 2))), cvtZToInt(strPtr(strSubN(time, 11, 2))),
//             cvtZToInt(strPtr(strSubN(time, 14, 2))), cvtZToInt(strPtr(strSubN(time, 17, 2))), 0));
// }

// static void
// storageAzureInfoListCallback(StorageAzure *this, void *callbackData, const String *name, StorageType type, const XmlNode *xml)
// {
//     FUNCTION_TEST_BEGIN();
//         FUNCTION_TEST_PARAM(STORAGE_AZURE, this);
//         FUNCTION_TEST_PARAM_P(VOID, callbackData);
//         FUNCTION_TEST_PARAM(STRING, name);
//         FUNCTION_TEST_PARAM(ENUM, type);
//         FUNCTION_TEST_PARAM(XML_NODE, xml);
//     FUNCTION_TEST_END();
//
//     (void)this;
//     ASSERT(callbackData != NULL);
//     ASSERT(name != NULL);
//     ASSERT(xml != NULL);
//
//     StorageAzureInfoListData *data = (StorageAzureInfoListData *)callbackData;
//
//     StorageInfo info =
//     {
//         .name = name,
//         .level = data->level,
//         .exists = true,
//     };
//
//     if (data->level >= storageInfoLevelBasic)
//     {
//         info.type = type;
//         info.size = type == storageTypeFile ?
//             cvtZToUInt64(strPtr(xmlNodeContent(xmlNodeChild(xml, AZURE_XML_TAG_SIZE_STR, true)))) : 0;
//         info.timeModified = type == storageTypeFile ?
//             storageAzureCvtTime(xmlNodeContent(xmlNodeChild(xml, AZURE_XML_TAG_LAST_MODIFIED_STR, true))) : 0;
//     }
//
//     data->callback(data->callbackData, &info);
//
//     FUNCTION_TEST_RETURN_VOID();
// }

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

    // MEM_CONTEXT_TEMP_BEGIN()
    // {
    //     StorageAzureInfoListData data = {.level = level, .callback = callback, .callbackData = callbackData};
    //     storageAzureListInternal(this, path, param.expression, false, storageAzureInfoListCallback, &data);
    // }
    // MEM_CONTEXT_TEMP_END();

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
    MemContext *memContext;                                         // Mem context to create xml document in
    unsigned int size;                                              // Size of delete request
    XmlDocument *xml;                                               // Delete request
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
//         this, HTTP_VERB_POST_STR, FSLASH_STR, httpQueryAdd(httpQueryNew(), AZURE_QUERY_DELETE_STR, EMPTY_STR),
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

// static void
// storageAzurePathRemoveCallback(StorageAzure *this, void *callbackData, const String *name, StorageType type, const XmlNode *xml)
// {
//     FUNCTION_TEST_BEGIN();
//         FUNCTION_TEST_PARAM(STORAGE_AZURE, this);
//         FUNCTION_TEST_PARAM_P(VOID, callbackData);
//         FUNCTION_TEST_PARAM(STRING, name);
//         FUNCTION_TEST_PARAM(ENUM, type);
//         FUNCTION_TEST_PARAM(XML_NODE, xml);
//     FUNCTION_TEST_END();
//
//     ASSERT(this != NULL);
//     ASSERT(callbackData != NULL);
//     (void)name;
//     ASSERT(xml != NULL);
//
//     // Only delete files since paths don't really exist
//     if (type == storageTypeFile)
//     {
//         StorageAzurePathRemoveData *data = (StorageAzurePathRemoveData *)callbackData;
//
//         // If there is something to delete then create the request
//         if (data->xml == NULL)
//         {
//             MEM_CONTEXT_BEGIN(data->memContext)
//             {
//                 data->xml = xmlDocumentNew(AZURE_XML_TAG_DELETE_STR);
//                 xmlNodeContentSet(xmlNodeAdd(xmlDocumentRoot(data->xml), AZURE_XML_TAG_QUIET_STR), TRUE_STR);
//             }
//             MEM_CONTEXT_END();
//         }
//
//         // Add to delete list
//         xmlNodeContentSet(
//             xmlNodeAdd(xmlNodeAdd(xmlDocumentRoot(data->xml), AZURE_XML_TAG_OBJECT_STR), AZURE_XML_TAG_KEY_STR),
//             xmlNodeContent(xmlNodeChild(xml, AZURE_XML_TAG_KEY_STR, true)));
//         data->size++;
//
//         // Delete list when it is full
//         if (data->size == this->deleteMax)
//         {
//             storageAzurePathRemoveInternal(this, data->xml);
//
//             xmlDocumentFree(data->xml);
//             data->xml = NULL;
//             data->size = 0;
//         }
//     }
//
//     FUNCTION_TEST_RETURN_VOID();
// }

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

    // MEM_CONTEXT_TEMP_BEGIN()
    // {
    //     StorageAzurePathRemoveData data = {.memContext = memContextCurrent()};
    //     storageAzureListInternal(this, path, NULL, true, storageAzurePathRemoveCallback, &data);
    //
    //     if (data.xml != NULL)
    //         storageAzurePathRemoveInternal(this, data.xml);
    // }
    // MEM_CONTEXT_TEMP_END();

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

    // storageAzureRequest(this, HTTP_VERB_DELETE_STR, file, NULL, NULL, true, false);

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
            .pathStyle = host != NULL,

            // Force the signing key to be generated on the first run
            // !!! .signingKeyDate = YYYYMMDD_STR,
        };

        // Create the http client cache used to service requests
        driver->httpClientCache = httpClientCacheNew(
            host == NULL ? STRDEF("BOGUS") : host, port, timeout, verifyPeer, caFile, caPath);

        // Create list of redacted headers
        driver->headerRedactList = strLstNew();
        strLstAdd(driver->headerRedactList, AZURE_HEADER_AUTHORIZATION_STR);

        this = storageNew(
            STORAGE_AZURE_TYPE_STR, path, 0, 0, write, pathExpressionFunction, driver, driver->interface);
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_LOG_RETURN(STORAGE, this);
}
