/***********************************************************************************************************************************
S3 Storage
***********************************************************************************************************************************/
#include "build.auto.h"

#include <string.h>

#include "common/crypto/hash.h"
#include "common/encode.h"
#include "common/debug.h"
#include "common/io/http/client.h"
#include "common/io/http/common.h"
#include "common/io/socket/client.h"
#include "common/io/tls/client.h"
#include "common/log.h"
#include "common/memContext.h"
#include "common/regExp.h"
#include "common/type/object.h"
#include "common/type/xml.h"
#include "storage/s3/read.h"
#include "storage/s3/storage.intern.h"
#include "storage/s3/write.h"

/***********************************************************************************************************************************
Storage type
***********************************************************************************************************************************/
STRING_EXTERN(STORAGE_S3_TYPE_STR,                                  STORAGE_S3_TYPE);

/***********************************************************************************************************************************
S3 HTTP headers
***********************************************************************************************************************************/
STRING_STATIC(S3_HEADER_CONTENT_SHA256_STR,                         "x-amz-content-sha256");
STRING_STATIC(S3_HEADER_DATE_STR,                                   "x-amz-date");
STRING_STATIC(S3_HEADER_TOKEN_STR,                                  "x-amz-security-token");

/***********************************************************************************************************************************
S3 query tokens
***********************************************************************************************************************************/
STRING_STATIC(S3_QUERY_CONTINUATION_TOKEN_STR,                      "continuation-token");
STRING_STATIC(S3_QUERY_DELETE_STR,                                  "delete");
STRING_STATIC(S3_QUERY_DELIMITER_STR,                               "delimiter");
STRING_STATIC(S3_QUERY_LIST_TYPE_STR,                               "list-type");
STRING_STATIC(S3_QUERY_PREFIX_STR,                                  "prefix");

STRING_STATIC(S3_QUERY_VALUE_LIST_TYPE_2_STR,                       "2");

/***********************************************************************************************************************************
XML tags
***********************************************************************************************************************************/
STRING_STATIC(S3_XML_TAG_CODE_STR,                                  "Code");
STRING_STATIC(S3_XML_TAG_COMMON_PREFIXES_STR,                       "CommonPrefixes");
STRING_STATIC(S3_XML_TAG_CONTENTS_STR,                              "Contents");
STRING_STATIC(S3_XML_TAG_DELETE_STR,                                "Delete");
STRING_STATIC(S3_XML_TAG_ERROR_STR,                                 "Error");
STRING_STATIC(S3_XML_TAG_KEY_STR,                                   "Key");
STRING_STATIC(S3_XML_TAG_LAST_MODIFIED_STR,                         "LastModified");
STRING_STATIC(S3_XML_TAG_MESSAGE_STR,                               "Message");
STRING_STATIC(S3_XML_TAG_NEXT_CONTINUATION_TOKEN_STR,               "NextContinuationToken");
STRING_STATIC(S3_XML_TAG_OBJECT_STR,                                "Object");
STRING_STATIC(S3_XML_TAG_PREFIX_STR,                                "Prefix");
STRING_STATIC(S3_XML_TAG_QUIET_STR,                                 "Quiet");
STRING_STATIC(S3_XML_TAG_SIZE_STR,                                  "Size");

/***********************************************************************************************************************************
AWS authentication v4 constants
***********************************************************************************************************************************/
#define S3                                                          "s3"
    BUFFER_STRDEF_STATIC(S3_BUF,                                    S3);
#define AWS4                                                        "AWS4"
#define AWS4_REQUEST                                                "aws4_request"
    BUFFER_STRDEF_STATIC(AWS4_REQUEST_BUF,                          AWS4_REQUEST);
#define AWS4_HMAC_SHA256                                            "AWS4-HMAC-SHA256"

/***********************************************************************************************************************************
Starting date for signing string so it will be regenerated on the first request
***********************************************************************************************************************************/
STRING_STATIC(YYYYMMDD_STR,                                         "YYYYMMDD");

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct StorageS3
{
    STORAGE_COMMON_MEMBER;
    MemContext *memContext;
    HttpClient *httpClient;                                         // HTTP client to service requests
    StringList *headerRedactList;                                   // List of headers to redact from logging

    const String *bucket;                                           // Bucket to store data in
    const String *region;                                           // e.g. us-east-1
    const String *accessKey;                                        // Access key
    const String *secretAccessKey;                                  // Secret access key
    const String *securityToken;                                    // Security token, if any
    size_t partSize;                                                // Part size for multi-part upload
    unsigned int deleteMax;                                         // Maximum objects that can be deleted in one request
    StorageS3UriStyle uriStyle;                                     // Path or host style URIs
    const String *bucketEndpoint;                                   // Set to {bucket}.{endpoint}

    // Current signing key and date it is valid for
    const String *signingKeyDate;                                   // Date of cached signing key (so we know when to regenerate)
    const Buffer *signingKey;                                       // Cached signing key
};

/***********************************************************************************************************************************
Expected ISO-8601 data/time size
***********************************************************************************************************************************/
#define ISO_8601_DATE_TIME_SIZE                                     16

/***********************************************************************************************************************************
Format ISO-8601 date/time for authentication
***********************************************************************************************************************************/
static String *
storageS3DateTime(time_t authTime)
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

Based on the excellent documentation at http://docs.aws.amazon.com/AmazonS3/latest/API/sig-v4-header-based-auth.html.
***********************************************************************************************************************************/
static void
storageS3Auth(
    StorageS3 *this, const String *verb, const String *uri, const HttpQuery *query, const String *dateTime,
    HttpHeader *httpHeader, const String *payloadHash)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_S3, this);
        FUNCTION_TEST_PARAM(STRING, verb);
        FUNCTION_TEST_PARAM(STRING, uri);
        FUNCTION_TEST_PARAM(HTTP_QUERY, query);
        FUNCTION_TEST_PARAM(STRING, dateTime);
        FUNCTION_TEST_PARAM(KEY_VALUE, httpHeader);
        FUNCTION_TEST_PARAM(STRING, payloadHash);
    FUNCTION_TEST_END();

    ASSERT(verb != NULL);
    ASSERT(uri != NULL);
    ASSERT(dateTime != NULL);
    ASSERT(httpHeader != NULL);
    ASSERT(payloadHash != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Get date from datetime
        const String *date = strSubN(dateTime, 0, 8);

        // Set required headers
        httpHeaderPut(httpHeader, S3_HEADER_CONTENT_SHA256_STR, payloadHash);
        httpHeaderPut(httpHeader, S3_HEADER_DATE_STR, dateTime);
        httpHeaderPut(httpHeader, HTTP_HEADER_HOST_STR, this->bucketEndpoint);

        if (this->securityToken != NULL)
            httpHeaderPut(httpHeader, S3_HEADER_TOKEN_STR, this->securityToken);

        // Generate canonical request and signed headers
        const StringList *headerList = strLstSort(strLstDup(httpHeaderList(httpHeader)), sortOrderAsc);
        String *signedHeaders = NULL;

        String *canonicalRequest = strNewFmt(
            "%s\n%s\n%s\n", strZ(verb), strZ(uri), query == NULL ? "" : strZ(httpQueryRenderP(query)));

        for (unsigned int headerIdx = 0; headerIdx < strLstSize(headerList); headerIdx++)
        {
            const String *headerKey = strLstGet(headerList, headerIdx);
            const String *headerKeyLower = strLower(strDup(headerKey));

            // Skip the authorization header -- if it exists this is a retry
            if (strEq(headerKeyLower, HTTP_HEADER_AUTHORIZATION_STR))
                continue;

            strCatFmt(canonicalRequest, "%s:%s\n", strZ(headerKeyLower), strZ(httpHeaderGet(httpHeader, headerKey)));

            if (signedHeaders == NULL)
                signedHeaders = strDup(headerKeyLower);
            else
                strCatFmt(signedHeaders, ";%s", strZ(headerKeyLower));
        }

        strCatFmt(canonicalRequest, "\n%s\n%s", strZ(signedHeaders), strZ(payloadHash));

        // Generate string to sign
        const String *stringToSign = strNewFmt(
            AWS4_HMAC_SHA256 "\n%s\n%s/%s/" S3 "/" AWS4_REQUEST "\n%s", strZ(dateTime), strZ(date), strZ(this->region),
            strZ(bufHex(cryptoHashOne(HASH_TYPE_SHA256_STR, BUFSTR(canonicalRequest)))));

        // Generate signing key.  This key only needs to be regenerated every seven days but we'll do it once a day to keep the
        // logic simple.  It's a relatively expensive operation so we'd rather not do it for every request.
        // If the cached signing key has expired (or has none been generated) then regenerate it
        if (!strEq(date, this->signingKeyDate))
        {
            const Buffer *dateKey = cryptoHmacOne(
                HASH_TYPE_SHA256_STR, BUFSTR(strNewFmt(AWS4 "%s", strZ(this->secretAccessKey))), BUFSTR(date));
            const Buffer *regionKey = cryptoHmacOne(HASH_TYPE_SHA256_STR, dateKey, BUFSTR(this->region));
            const Buffer *serviceKey = cryptoHmacOne(HASH_TYPE_SHA256_STR, regionKey, S3_BUF);

            // Switch to the object context so signing key and date are not lost
            MEM_CONTEXT_BEGIN(this->memContext)
            {
                this->signingKey = cryptoHmacOne(HASH_TYPE_SHA256_STR, serviceKey, AWS4_REQUEST_BUF);
                this->signingKeyDate = strDup(date);
            }
            MEM_CONTEXT_END();
        }

        // Generate authorization header
        const String *authorization = strNewFmt(
            AWS4_HMAC_SHA256 " Credential=%s/%s/%s/" S3 "/" AWS4_REQUEST ",SignedHeaders=%s,Signature=%s",
            strZ(this->accessKey), strZ(date), strZ(this->region), strZ(signedHeaders),
            strZ(bufHex(cryptoHmacOne(HASH_TYPE_SHA256_STR, this->signingKey, BUFSTR(stringToSign)))));

        httpHeaderPut(httpHeader, HTTP_HEADER_AUTHORIZATION_STR, authorization);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_TEST_RETURN_VOID();
}

/***********************************************************************************************************************************
Process S3 request
***********************************************************************************************************************************/
HttpRequest *
storageS3RequestAsync(StorageS3 *this, const String *verb, const String *uri, StorageS3RequestAsyncParam param)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STORAGE_S3, this);
        FUNCTION_LOG_PARAM(STRING, verb);
        FUNCTION_LOG_PARAM(STRING, uri);
        FUNCTION_LOG_PARAM(HTTP_QUERY, param.query);
        FUNCTION_LOG_PARAM(BUFFER, param.content);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(verb != NULL);
    ASSERT(uri != NULL);

    HttpRequest *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        HttpHeader *requestHeader = httpHeaderNew(this->headerRedactList);

        // Set content length
        httpHeaderAdd(
            requestHeader, HTTP_HEADER_CONTENT_LENGTH_STR,
            param.content == NULL || bufUsed(param.content) == 0 ? ZERO_STR : strNewFmt("%zu", bufUsed(param.content)));

        // Calculate content-md5 header if there is content
        if (param.content != NULL)
        {
            char md5Hash[HASH_TYPE_MD5_SIZE_HEX];
            encodeToStr(encodeBase64, bufPtr(cryptoHashOne(HASH_TYPE_MD5_STR, param.content)), HASH_TYPE_M5_SIZE, md5Hash);
            httpHeaderAdd(requestHeader, HTTP_HEADER_CONTENT_MD5_STR, STR(md5Hash));
        }

        // When using path-style URIs the bucket name needs to be prepended
        if (this->uriStyle == storageS3UriStylePath)
            uri = strNewFmt("/%s%s", strZ(this->bucket), strZ(uri));

        // Generate authorization header
        storageS3Auth(
            this, verb, httpUriEncode(uri, true), param.query, storageS3DateTime(time(NULL)), requestHeader,
            param.content == NULL || bufUsed(param.content) == 0 ?
                HASH_TYPE_SHA256_ZERO_STR : bufHex(cryptoHashOne(HASH_TYPE_SHA256_STR, param.content)));

        // Send request
        MEM_CONTEXT_PRIOR_BEGIN()
        {
            result = httpRequestNewP(
                this->httpClient, verb, uri, .query = param.query, .header = requestHeader, .content = param.content);
        }
        MEM_CONTEXT_END();
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(HTTP_REQUEST, result);
}

HttpResponse *
storageS3Response(HttpRequest *request, StorageS3ResponseParam param)
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
storageS3Request(StorageS3 *this, const String *verb, const String *uri, StorageS3RequestParam param)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STORAGE_S3, this);
        FUNCTION_LOG_PARAM(STRING, verb);
        FUNCTION_LOG_PARAM(STRING, uri);
        FUNCTION_LOG_PARAM(HTTP_QUERY, param.query);
        FUNCTION_LOG_PARAM(BUFFER, param.content);
        FUNCTION_LOG_PARAM(BOOL, param.allowMissing);
        FUNCTION_LOG_PARAM(BOOL, param.contentIo);
    FUNCTION_LOG_END();

    FUNCTION_LOG_RETURN(
        HTTP_RESPONSE,
        storageS3ResponseP(
            storageS3RequestAsyncP(this, verb, uri, .query = param.query, .content = param.content),
            .allowMissing = param.allowMissing, .contentIo = param.contentIo));
}

/***********************************************************************************************************************************
General function for listing files to be used by other list routines
***********************************************************************************************************************************/
static void
storageS3ListInternal(
    StorageS3 *this, const String *path, const String *expression, bool recurse,
    void (*callback)(StorageS3 *this, void *callbackData, const String *name, StorageType type, const XmlNode *xml),
    void *callbackData)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STORAGE_S3, this);
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
        const String *continuationToken = NULL;

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

        // Loop as long as a continuation token returned
        do
        {
            // Use an inner mem context here because we could potentially be retrieving millions of files so it is a good idea to
            // free memory at regular intervals
            MEM_CONTEXT_TEMP_BEGIN()
            {
                HttpQuery *query = httpQueryNewP();

                // Add continuation token from the prior loop if any
                if (continuationToken != NULL)
                    httpQueryAdd(query, S3_QUERY_CONTINUATION_TOKEN_STR, continuationToken);

                // Add the delimiter to not recurse
                if (!recurse)
                    httpQueryAdd(query, S3_QUERY_DELIMITER_STR, FSLASH_STR);

                // Use list type 2
                httpQueryAdd(query, S3_QUERY_LIST_TYPE_STR, S3_QUERY_VALUE_LIST_TYPE_2_STR);

                // Don't specify empty prefix because it is the default
                if (!strEmpty(queryPrefix))
                    httpQueryAdd(query, S3_QUERY_PREFIX_STR, queryPrefix);

                XmlNode *xmlRoot = xmlDocumentRoot(
                    xmlDocumentNewBuf(httpResponseContent(storageS3RequestP(this, HTTP_VERB_GET_STR, FSLASH_STR, query))));

                // Get subpath list
                XmlNodeList *subPathList = xmlNodeChildList(xmlRoot, S3_XML_TAG_COMMON_PREFIXES_STR);

                for (unsigned int subPathIdx = 0; subPathIdx < xmlNodeLstSize(subPathList); subPathIdx++)
                {
                    const XmlNode *subPathNode = xmlNodeLstGet(subPathList, subPathIdx);

                    // Get subpath name
                    const String *subPath = xmlNodeContent(xmlNodeChild(subPathNode, S3_XML_TAG_PREFIX_STR, true));

                    // Strip off base prefix and final /
                    subPath = strSubN(subPath, strSize(basePrefix), strSize(subPath) - strSize(basePrefix) - 1);

                    // Add to list
                    callback(this, callbackData, subPath, storageTypePath, NULL);
                }

                // Get file list
                XmlNodeList *fileList = xmlNodeChildList(xmlRoot, S3_XML_TAG_CONTENTS_STR);

                for (unsigned int fileIdx = 0; fileIdx < xmlNodeLstSize(fileList); fileIdx++)
                {
                    const XmlNode *fileNode = xmlNodeLstGet(fileList, fileIdx);

                    // Get file name
                    const String *file = xmlNodeContent(xmlNodeChild(fileNode, S3_XML_TAG_KEY_STR, true));

                    // Strip off the base prefix when present
                    file = strEmpty(basePrefix) ? file : strSub(file, strSize(basePrefix));

                    // Add to list
                    callback(this, callbackData, file, storageTypeFile, fileNode);
                }

                // Get the continuation token and store it in the outer temp context
                MEM_CONTEXT_PRIOR_BEGIN()
                {
                    continuationToken = xmlNodeContent(xmlNodeChild(xmlRoot, S3_XML_TAG_NEXT_CONTINUATION_TOKEN_STR, false));
                }
                MEM_CONTEXT_PRIOR_END();
            }
            MEM_CONTEXT_TEMP_END();
        }
        while (continuationToken != NULL);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
static StorageInfo
storageS3Info(THIS_VOID, const String *file, StorageInfoLevel level, StorageInterfaceInfoParam param)
{
    THIS(StorageS3);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_S3, this);
        FUNCTION_LOG_PARAM(STRING, file);
        FUNCTION_LOG_PARAM(ENUM, level);
        (void)param;                                                // No parameters are used
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(file != NULL);

    // Attempt to get file info
    HttpResponse *httpResponse = storageS3RequestP(this, HTTP_VERB_HEAD_STR, file, .allowMissing = true);

    // Does the file exist?
    StorageInfo result = {.level = level, .exists = httpResponseCodeOk(httpResponse)};

    // Add basic level info if requested and the file exists
    if (result.level >= storageInfoLevelBasic && result.exists)
    {
        const HttpHeader *httpHeader = httpResponseHeader(httpResponse);

        result.type = storageTypeFile;
        result.size = cvtZToUInt64(strZ(httpHeaderGet(httpHeader, HTTP_HEADER_CONTENT_LENGTH_STR)));
        result.timeModified = httpDateToTime(httpHeaderGet(httpHeader, HTTP_HEADER_LAST_MODIFIED_STR));
    }

    FUNCTION_LOG_RETURN(STORAGE_INFO, result);
}

/**********************************************************************************************************************************/
typedef struct StorageS3InfoListData
{
    StorageInfoLevel level;                                         // Level of info to set
    StorageInfoListCallback callback;                               // User-supplied callback function
    void *callbackData;                                             // User-supplied callback data
} StorageS3InfoListData;

// Helper to convert YYYY-MM-DDTHH:MM:SS.MSECZ format to time_t.  This format is very nearly ISO-8601 except for the inclusion of
// milliseconds which are discarded here.
static time_t
storageS3CvtTime(const String *time)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, time);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RETURN(
        epochFromParts(
            cvtZToInt(strZ(strSubN(time, 0, 4))), cvtZToInt(strZ(strSubN(time, 5, 2))),
            cvtZToInt(strZ(strSubN(time, 8, 2))), cvtZToInt(strZ(strSubN(time, 11, 2))),
            cvtZToInt(strZ(strSubN(time, 14, 2))), cvtZToInt(strZ(strSubN(time, 17, 2))), 0));
}

static void
storageS3InfoListCallback(StorageS3 *this, void *callbackData, const String *name, StorageType type, const XmlNode *xml)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_S3, this);
        FUNCTION_TEST_PARAM_P(VOID, callbackData);
        FUNCTION_TEST_PARAM(STRING, name);
        FUNCTION_TEST_PARAM(ENUM, type);
        FUNCTION_TEST_PARAM(XML_NODE, xml);
    FUNCTION_TEST_END();

    (void)this;                                                     // Unused but still logged above for debugging
    ASSERT(callbackData != NULL);
    ASSERT(name != NULL);

    StorageS3InfoListData *data = (StorageS3InfoListData *)callbackData;

    StorageInfo info =
    {
        .name = name,
        .level = data->level,
        .exists = true,
    };

    if (data->level >= storageInfoLevelBasic)
    {
        info.type = type;

        // Add additional info for files
        if (type == storageTypeFile)
        {
            ASSERT(xml != NULL);

            info.size = cvtZToUInt64(strZ(xmlNodeContent(xmlNodeChild(xml, S3_XML_TAG_SIZE_STR, true))));
            info.timeModified = storageS3CvtTime(xmlNodeContent(xmlNodeChild(xml, S3_XML_TAG_LAST_MODIFIED_STR, true)));
        }
    }

    data->callback(data->callbackData, &info);

    FUNCTION_TEST_RETURN_VOID();
}

static bool
storageS3InfoList(
    THIS_VOID, const String *path, StorageInfoLevel level, StorageInfoListCallback callback, void *callbackData,
    StorageInterfaceInfoListParam param)
{
    THIS(StorageS3);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_S3, this);
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
        StorageS3InfoListData data = {.level = level, .callback = callback, .callbackData = callbackData};
        storageS3ListInternal(this, path, param.expression, false, storageS3InfoListCallback, &data);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(BOOL, true);
}

/**********************************************************************************************************************************/
static StorageRead *
storageS3NewRead(THIS_VOID, const String *file, bool ignoreMissing, StorageInterfaceNewReadParam param)
{
    THIS(StorageS3);

    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STORAGE_S3, this);
        FUNCTION_LOG_PARAM(STRING, file);
        FUNCTION_LOG_PARAM(BOOL, ignoreMissing);
        (void)param;                                                // No parameters are used
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(file != NULL);

    FUNCTION_LOG_RETURN(STORAGE_READ, storageReadS3New(this, file, ignoreMissing));
}

/**********************************************************************************************************************************/
static StorageWrite *
storageS3NewWrite(THIS_VOID, const String *file, StorageInterfaceNewWriteParam param)
{
    THIS(StorageS3);

    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STORAGE_S3, this);
        FUNCTION_LOG_PARAM(STRING, file);
        (void)param;                                                // No parameters are used
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(file != NULL);
    ASSERT(param.createPath);
    ASSERT(param.user == NULL);
    ASSERT(param.group == NULL);
    ASSERT(param.timeModified == 0);

    FUNCTION_LOG_RETURN(STORAGE_WRITE, storageWriteS3New(this, file, this->partSize));
}

/**********************************************************************************************************************************/
typedef struct StorageS3PathRemoveData
{
    MemContext *memContext;                                         // Mem context to create xml document in
    unsigned int size;                                              // Size of delete request
    XmlDocument *xml;                                               // Delete request
} StorageS3PathRemoveData;

static void
storageS3PathRemoveInternal(StorageS3 *this, XmlDocument *request)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_S3, this);
        FUNCTION_TEST_PARAM(XML_DOCUMENT, request);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(request != NULL);

    const Buffer *response = httpResponseContent(
        storageS3RequestP(
            this, HTTP_VERB_POST_STR, FSLASH_STR, .query = httpQueryAdd(httpQueryNewP(), S3_QUERY_DELETE_STR, EMPTY_STR),
            .content = xmlDocumentBuf(request)));

    // Nothing is returned when there are no errors
    if (bufSize(response) > 0)
    {
        XmlNodeList *errorList = xmlNodeChildList(xmlDocumentRoot(xmlDocumentNewBuf(response)), S3_XML_TAG_ERROR_STR);

        if (xmlNodeLstSize(errorList) > 0)
        {
            XmlNode *error = xmlNodeLstGet(errorList, 0);

            THROW_FMT(
                FileRemoveError, STORAGE_ERROR_PATH_REMOVE_FILE ": [%s] %s",
                strZ(xmlNodeContent(xmlNodeChild(error, S3_XML_TAG_KEY_STR, true))),
                strZ(xmlNodeContent(xmlNodeChild(error, S3_XML_TAG_CODE_STR, true))),
                strZ(xmlNodeContent(xmlNodeChild(error, S3_XML_TAG_MESSAGE_STR, true))));
        }
    }

    FUNCTION_TEST_RETURN_VOID();
}

static void
storageS3PathRemoveCallback(StorageS3 *this, void *callbackData, const String *name, StorageType type, const XmlNode *xml)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_S3, this);
        FUNCTION_TEST_PARAM_P(VOID, callbackData);
        (void)name;                                                 // Unused since full path from XML needed
        FUNCTION_TEST_PARAM(ENUM, type);
        FUNCTION_TEST_PARAM(XML_NODE, xml);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(callbackData != NULL);

    // Only delete files since paths don't really exist
    if (type == storageTypeFile)
    {
        ASSERT(xml != NULL);

        StorageS3PathRemoveData *data = (StorageS3PathRemoveData *)callbackData;

        // If there is something to delete then create the request
        if (data->xml == NULL)
        {
            MEM_CONTEXT_BEGIN(data->memContext)
            {
                data->xml = xmlDocumentNew(S3_XML_TAG_DELETE_STR);
                xmlNodeContentSet(xmlNodeAdd(xmlDocumentRoot(data->xml), S3_XML_TAG_QUIET_STR), TRUE_STR);
            }
            MEM_CONTEXT_END();
        }

        // Add to delete list
        xmlNodeContentSet(
            xmlNodeAdd(xmlNodeAdd(xmlDocumentRoot(data->xml), S3_XML_TAG_OBJECT_STR), S3_XML_TAG_KEY_STR),
            xmlNodeContent(xmlNodeChild(xml, S3_XML_TAG_KEY_STR, true)));
        data->size++;

        // Delete list when it is full
        if (data->size == this->deleteMax)
        {
            storageS3PathRemoveInternal(this, data->xml);

            xmlDocumentFree(data->xml);
            data->xml = NULL;
            data->size = 0;
        }
    }

    FUNCTION_TEST_RETURN_VOID();
}

static bool
storageS3PathRemove(THIS_VOID, const String *path, bool recurse, StorageInterfacePathRemoveParam param)
{
    THIS(StorageS3);

    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STORAGE_S3, this);
        FUNCTION_LOG_PARAM(STRING, path);
        FUNCTION_LOG_PARAM(BOOL, recurse);
        (void)param;                                                // No parameters are used
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(path != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        StorageS3PathRemoveData data = {.memContext = memContextCurrent()};
        storageS3ListInternal(this, path, NULL, true, storageS3PathRemoveCallback, &data);

        if (data.xml != NULL)
            storageS3PathRemoveInternal(this, data.xml);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(BOOL, true);
}

/**********************************************************************************************************************************/
static void
storageS3Remove(THIS_VOID, const String *file, StorageInterfaceRemoveParam param)
{
    THIS(StorageS3);

    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STORAGE_S3, this);
        FUNCTION_LOG_PARAM(STRING, file);
        FUNCTION_LOG_PARAM(BOOL, param.errorOnMissing);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(file != NULL);
    ASSERT(!param.errorOnMissing);

    storageS3RequestP(this, HTTP_VERB_DELETE_STR, file);

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
static const StorageInterface storageInterfaceS3 =
{
    .info = storageS3Info,
    .infoList = storageS3InfoList,
    .newRead = storageS3NewRead,
    .newWrite = storageS3NewWrite,
    .pathRemove = storageS3PathRemove,
    .remove = storageS3Remove,
};

Storage *
storageS3New(
    const String *path, bool write, StoragePathExpressionCallback pathExpressionFunction, const String *bucket,
    const String *endPoint, StorageS3UriStyle uriStyle, const String *region, const String *accessKey,
    const String *secretAccessKey, const String *securityToken, size_t partSize, unsigned int deleteMax, const String *host,
    unsigned int port, TimeMSec timeout, bool verifyPeer, const String *caFile, const String *caPath)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STRING, path);
        FUNCTION_LOG_PARAM(BOOL, write);
        FUNCTION_LOG_PARAM(FUNCTIONP, pathExpressionFunction);
        FUNCTION_LOG_PARAM(STRING, bucket);
        FUNCTION_LOG_PARAM(STRING, endPoint);
        FUNCTION_LOG_PARAM(ENUM, uriStyle);
        FUNCTION_LOG_PARAM(STRING, region);
        FUNCTION_TEST_PARAM(STRING, accessKey);
        FUNCTION_TEST_PARAM(STRING, secretAccessKey);
        FUNCTION_TEST_PARAM(STRING, securityToken);
        FUNCTION_LOG_PARAM(SIZE, partSize);
        FUNCTION_LOG_PARAM(STRING, host);
        FUNCTION_LOG_PARAM(UINT, port);
        FUNCTION_LOG_PARAM(TIME_MSEC, timeout);
        FUNCTION_LOG_PARAM(BOOL, verifyPeer);
        FUNCTION_LOG_PARAM(STRING, caFile);
        FUNCTION_LOG_PARAM(STRING, caPath);
    FUNCTION_LOG_END();

    ASSERT(path != NULL);
    ASSERT(bucket != NULL);
    ASSERT(endPoint != NULL);
    ASSERT(region != NULL);
    ASSERT(accessKey != NULL);
    ASSERT(secretAccessKey != NULL);
    ASSERT(partSize != 0);

    Storage *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("StorageS3")
    {
        StorageS3 *driver = memNew(sizeof(StorageS3));

        *driver = (StorageS3)
        {
            .memContext = MEM_CONTEXT_NEW(),
            .interface = storageInterfaceS3,
            .bucket = strDup(bucket),
            .region = strDup(region),
            .accessKey = strDup(accessKey),
            .secretAccessKey = strDup(secretAccessKey),
            .securityToken = strDup(securityToken),
            .partSize = partSize,
            .deleteMax = deleteMax,
            .uriStyle = uriStyle,
            .bucketEndpoint = uriStyle == storageS3UriStyleHost ?
                strNewFmt("%s.%s", strZ(bucket), strZ(endPoint)) : strDup(endPoint),

            // Force the signing key to be generated on the first run
            .signingKeyDate = YYYYMMDD_STR,
        };

        // Create the HTTP client used to service requests
        if (host == NULL)
            host = driver->bucketEndpoint;

        driver->httpClient = httpClientNew(
            tlsClientNew(sckClientNew(host, port, timeout), host, timeout, verifyPeer, caFile, caPath), timeout);

        // Create list of redacted headers
        driver->headerRedactList = strLstNew();
        strLstAdd(driver->headerRedactList, HTTP_HEADER_AUTHORIZATION_STR);
        strLstAdd(driver->headerRedactList, S3_HEADER_DATE_STR);

        this = storageNew(
            STORAGE_S3_TYPE_STR, path, 0, 0, write, pathExpressionFunction, driver, driver->interface);
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_LOG_RETURN(STORAGE, this);
}
