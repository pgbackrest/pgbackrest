/***********************************************************************************************************************************
S3 Storage
***********************************************************************************************************************************/
#include "build.auto.h"

#include <string.h>
#include <time.h>

#include "common/crypto/hash.h"
#include "common/encode.h"
#include "common/debug.h"
#include "common/io/http/common.h"
#include "common/log.h"
#include "common/memContext.h"
#include "common/object.h"
#include "common/regExp.h"
#include "common/type/xml.h"
#include "storage/s3/read.h"
#include "storage/s3/storage.intern.h"
#include "storage/s3/write.h"

/***********************************************************************************************************************************
Storage type
***********************************************************************************************************************************/
STRING_EXTERN(STORAGE_S3_TYPE_STR,                                  STORAGE_S3_TYPE);

/***********************************************************************************************************************************
S3 http headers
***********************************************************************************************************************************/
STRING_STATIC(S3_HEADER_AUTHORIZATION_STR,                          "authorization");
STRING_STATIC(S3_HEADER_HOST_STR,                                   "host");
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
STRING_STATIC(S3_XML_TAG_MESSAGE_STR,                               "Message");
STRING_STATIC(S3_XML_TAG_NEXT_CONTINUATION_TOKEN_STR,               "NextContinuationToken");
STRING_STATIC(S3_XML_TAG_OBJECT_STR,                                "Object");
STRING_STATIC(S3_XML_TAG_PREFIX_STR,                                "Prefix");
STRING_STATIC(S3_XML_TAG_QUIET_STR,                                 "Quiet");

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
    MemContext *memContext;
    HttpClient *httpClient;                                         // Http client to service requests
    const StringList *headerRedactList;                             // List of headers to redact from logging

    const String *bucket;                                           // Bucket to store data in
    const String *region;                                           // e.g. us-east-1
    const String *accessKey;                                        // Access key
    const String *secretAccessKey;                                  // Secret access key
    const String *securityToken;                                    // Security token, if any
    size_t partSize;                                                // Part size for multi-part upload
    const String *host;                                             // Defaults to {bucket}.{endpoint}
    unsigned int port;                                              // Host port

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
        FUNCTION_TEST_PARAM(INT64, authTime);
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
        httpHeaderPut(httpHeader, S3_HEADER_HOST_STR, this->host);

        if (this->securityToken != NULL)
            httpHeaderPut(httpHeader, S3_HEADER_TOKEN_STR, this->securityToken);

        // Generate canonical request and signed headers
        const StringList *headerList = strLstSort(strLstDup(httpHeaderList(httpHeader)), sortOrderAsc);
        String *signedHeaders = NULL;

        String *canonicalRequest = strNewFmt(
            "%s\n%s\n%s\n", strPtr(verb), strPtr(uri), query == NULL ? "" : strPtr(httpQueryRender(query)));

        for (unsigned int headerIdx = 0; headerIdx < strLstSize(headerList); headerIdx++)
        {
            const String *headerKey = strLstGet(headerList, headerIdx);
            const String *headerKeyLower = strLower(strDup(headerKey));

            // Skip the authorization header -- if it exists this is a retry
            if (strEq(headerKeyLower, S3_HEADER_AUTHORIZATION_STR))
                continue;

            strCatFmt(canonicalRequest, "%s:%s\n", strPtr(headerKeyLower), strPtr(httpHeaderGet(httpHeader, headerKey)));

            if (signedHeaders == NULL)
                signedHeaders = strDup(headerKeyLower);
            else
                strCatFmt(signedHeaders, ";%s", strPtr(headerKeyLower));
        }

        strCatFmt(canonicalRequest, "\n%s\n%s", strPtr(signedHeaders), strPtr(payloadHash));

        // Generate string to sign
        const String *stringToSign = strNewFmt(
            AWS4_HMAC_SHA256 "\n%s\n%s/%s/" S3 "/" AWS4_REQUEST "\n%s", strPtr(dateTime), strPtr(date), strPtr(this->region),
            strPtr(bufHex(cryptoHashOne(HASH_TYPE_SHA256_STR, BUFSTR(canonicalRequest)))));

        // Generate signing key.  This key only needs to be regenerated every seven days but we'll do it once a day to keep the
        // logic simple.  It's a relatively expensive operation so we'd rather not do it for every request.
        // If the cached signing key has expired (or has none been generated) then regenerate it
        if (!strEq(date, this->signingKeyDate))
        {
            const Buffer *dateKey = cryptoHmacOne(
                HASH_TYPE_SHA256_STR, BUFSTR(strNewFmt(AWS4 "%s", strPtr(this->secretAccessKey))), BUFSTR(date));
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
            strPtr(this->accessKey), strPtr(date), strPtr(this->region), strPtr(signedHeaders),
            strPtr(bufHex(cryptoHmacOne(HASH_TYPE_SHA256_STR, this->signingKey, BUFSTR(stringToSign)))));

        httpHeaderPut(httpHeader, S3_HEADER_AUTHORIZATION_STR, authorization);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_TEST_RETURN_VOID();
}

/***********************************************************************************************************************************
Process S3 request
***********************************************************************************************************************************/
StorageS3RequestResult
storageS3Request(
    StorageS3 *this, const String *verb, const String *uri, const HttpQuery *query, const Buffer *body, bool returnContent,
    bool allowMissing)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STORAGE_S3, this);
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

    StorageS3RequestResult result = {0};

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Create header list and add content length
        HttpHeader *requestHeader = httpHeaderNew(this->headerRedactList);

        // Set content length
        httpHeaderAdd(
            requestHeader, HTTP_HEADER_CONTENT_LENGTH_STR,
            body == NULL || bufUsed(body) == 0 ? ZERO_STR : strNewFmt("%zu", bufUsed(body)));

        // Calculate content-md5 header if there is content
        if (body != NULL)
        {
            char md5Hash[HASH_TYPE_MD5_SIZE_HEX];
            encodeToStr(encodeBase64, bufPtr(cryptoHashOne(HASH_TYPE_MD5_STR, body)), HASH_TYPE_M5_SIZE, md5Hash);
            httpHeaderAdd(requestHeader, HTTP_HEADER_CONTENT_MD5_STR, STR(md5Hash));
        }

        // Generate authorization header
        storageS3Auth(
            this, verb, httpUriEncode(uri, true), query, storageS3DateTime(time(NULL)), requestHeader,
            body == NULL || bufUsed(body) == 0 ?
                STRDEF("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855") :
                bufHex(cryptoHashOne(HASH_TYPE_SHA256_STR, body)));

        // Process request
        Buffer *response = httpClientRequest(this->httpClient, verb, uri, query, requestHeader, body, returnContent);

        // Error if the request was not successful
        if (!httpClientResponseCodeOk(this->httpClient) &&
            (!allowMissing || httpClientResponseCode(this->httpClient) != HTTP_RESPONSE_CODE_NOT_FOUND))
        {
            // General error message
            String *error = strNewFmt(
                "S3 request failed with %u: %s", httpClientResponseCode(this->httpClient),
                strPtr(httpClientResponseMessage(this->httpClient)));

            // Output uri/query
            strCat(error, "\n*** URI/Query ***:");

            strCatFmt(error, "\n%s", strPtr(httpUriEncode(uri, true)));

            if (query != NULL)
                strCatFmt(error, "?%s", strPtr(httpQueryRender(query)));

            // Output request headers
            const StringList *requestHeaderList = httpHeaderList(requestHeader);

            strCat(error, "\n*** Request Headers ***:");

            for (unsigned int requestHeaderIdx = 0; requestHeaderIdx < strLstSize(requestHeaderList); requestHeaderIdx++)
            {
                const String *key = strLstGet(requestHeaderList, requestHeaderIdx);

                strCatFmt(
                    error, "\n%s: %s", strPtr(key),
                    httpHeaderRedact(requestHeader, key) || strEq(key, S3_HEADER_DATE_STR) ?
                        "<redacted>" : strPtr(httpHeaderGet(requestHeader, key)));
            }

            // Output response headers
            const HttpHeader *responseHeader = httpClientReponseHeader(this->httpClient);
            const StringList *responseHeaderList = httpHeaderList(responseHeader);

            if (strLstSize(responseHeaderList) > 0)
            {
                strCat(error, "\n*** Response Headers ***:");

                for (unsigned int responseHeaderIdx = 0; responseHeaderIdx < strLstSize(responseHeaderList); responseHeaderIdx++)
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

        // On success move the buffer to the calling context
        result.responseHeader = httpHeaderMove(httpHeaderDup(httpClientReponseHeader(this->httpClient), NULL), MEM_CONTEXT_OLD());
        result.response = bufMove(response, MEM_CONTEXT_OLD());
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(STORAGE_S3_REQUEST_RESULT, result);
}

/***********************************************************************************************************************************
Does a file exist? This function is only for files, not paths.
***********************************************************************************************************************************/
static bool
storageS3Exists(THIS_VOID, const String *path)
{
    THIS(StorageS3);

    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STORAGE_S3, this);
        FUNCTION_LOG_PARAM(STRING, path);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(path != NULL);

    bool result = false;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        HttpQuery *query = httpQueryNew();

        // Generate the file name as a prefix.  Muliple files may be returned but this will narrow down the list.
        String *prefix = strNewFmt("%s", strPtr(strSub(path, 1)));
        httpQueryAdd(query, S3_QUERY_PREFIX_STR, prefix);

        // Build the query using list type 2
        httpQueryAdd(query, S3_QUERY_LIST_TYPE_STR, S3_QUERY_VALUE_LIST_TYPE_2_STR);

        XmlNode *xmlRoot = xmlDocumentRoot(
            xmlDocumentNewBuf(storageS3Request(this, HTTP_VERB_GET_STR, FSLASH_STR, query, NULL, true, false).response));

        // Check if the prefix exists.  If not then the file definitely does not exist, but if it does we'll need to check the
        // exact name to be sure we are not looking at a different file with the same prefix
        XmlNodeList *fileList = xmlNodeChildList(xmlRoot, S3_XML_TAG_CONTENTS_STR);

        for (unsigned int fileIdx = 0; fileIdx < xmlNodeLstSize(fileList); fileIdx++)
        {
            // If the name matches exactly then the file exists
            if (strEq(prefix, xmlNodeContent(xmlNodeChild(xmlNodeLstGet(fileList, fileIdx), S3_XML_TAG_KEY_STR, true))))
            {
                result = true;
                break;
            }
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(BOOL, result);
}

/***********************************************************************************************************************************
File/path info
***********************************************************************************************************************************/
static StorageInfo
storageS3Info(THIS_VOID, const String *file, bool ignoreMissing, bool followLink)
{
    THIS(StorageS3);

    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STORAGE_S3, this);
        FUNCTION_LOG_PARAM(STRING, file);
        FUNCTION_LOG_PARAM(BOOL, ignoreMissing);
        FUNCTION_LOG_PARAM(BOOL, followLink);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(file != NULL);
    ASSERT(followLink == false);

    THROW(AssertError, "NOT YET IMPLEMENTED");

    FUNCTION_LOG_RETURN(STORAGE_INFO, (StorageInfo){0});
}

/***********************************************************************************************************************************
Get a list of files from a directory
***********************************************************************************************************************************/
static StringList *
storageS3List(THIS_VOID, const String *path, bool errorOnMissing, const String *expression)
{
    THIS(StorageS3);

    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STORAGE_S3, this);
        FUNCTION_LOG_PARAM(STRING, path);
        FUNCTION_LOG_PARAM(BOOL, errorOnMissing);
        FUNCTION_LOG_PARAM(STRING, expression);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(path != NULL);
    ASSERT(!errorOnMissing);

    StringList *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        result = strLstNew();
        const String *continuationToken = NULL;

        // Prepare regexp if an expression was passed
        RegExp *regExp = expression == NULL ? NULL : regExpNew(expression);

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
                if (continuationToken != NULL)
                    httpQueryAdd(query, S3_QUERY_CONTINUATION_TOKEN_STR, continuationToken);

                // Add the delimiter so we don't recurse
                httpQueryAdd(query, S3_QUERY_DELIMITER_STR, FSLASH_STR);

                // Use list type 2
                httpQueryAdd(query, S3_QUERY_LIST_TYPE_STR, S3_QUERY_VALUE_LIST_TYPE_2_STR);

                // Don't specified empty prefix because it is the default
                if (!strEmpty(queryPrefix))
                    httpQueryAdd(query, S3_QUERY_PREFIX_STR, queryPrefix);

                XmlNode *xmlRoot = xmlDocumentRoot(
                    xmlDocumentNewBuf(
                        storageS3Request(this, HTTP_VERB_GET_STR, FSLASH_STR, query, NULL, true, false).response));

                // Get subpath list
                XmlNodeList *subPathList = xmlNodeChildList(xmlRoot, S3_XML_TAG_COMMON_PREFIXES_STR);

                for (unsigned int subPathIdx = 0; subPathIdx < xmlNodeLstSize(subPathList); subPathIdx++)
                {
                    // Get subpath name
                    const String *subPath = xmlNodeContent(
                        xmlNodeChild(xmlNodeLstGet(subPathList, subPathIdx), S3_XML_TAG_PREFIX_STR, true));

                    // Strip off base prefix and final /
                    subPath = strSubN(subPath, strSize(basePrefix), strSize(subPath) - strSize(basePrefix) - 1);

                    // Add to list after checking expression if present
                    if (regExp == NULL || regExpMatch(regExp, subPath))
                        strLstAdd(result, subPath);
                }

                // Get file list
                XmlNodeList *fileList = xmlNodeChildList(xmlRoot, S3_XML_TAG_CONTENTS_STR);

                for (unsigned int fileIdx = 0; fileIdx < xmlNodeLstSize(fileList); fileIdx++)
                {
                    // Get file name
                    const String *file = xmlNodeContent(xmlNodeChild(xmlNodeLstGet(fileList, fileIdx), S3_XML_TAG_KEY_STR, true));

                    // Strip off the base prefix when present
                    file = strEmpty(basePrefix) ? file : strSub(file, strSize(basePrefix));

                    // Add to list after checking expression if present
                    if (regExp == NULL || regExpMatch(regExp, file))
                        strLstAdd(result, file);
                }

                // Get the continuation token and store it in the outer temp context
                memContextSwitch(MEM_CONTEXT_OLD());
                continuationToken = xmlNodeContent(xmlNodeChild(xmlRoot, S3_XML_TAG_NEXT_CONTINUATION_TOKEN_STR, false));
                memContextSwitch(MEM_CONTEXT_TEMP());
            }
            MEM_CONTEXT_TEMP_END();
        }
        while (continuationToken != NULL);

        strLstMove(result, MEM_CONTEXT_OLD());
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(STRING_LIST, result);
}

/***********************************************************************************************************************************
New file read object
***********************************************************************************************************************************/
static StorageRead *
storageS3NewRead(THIS_VOID, const String *file, bool ignoreMissing)
{
    THIS(StorageS3);

    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STORAGE_S3, this);
        FUNCTION_LOG_PARAM(STRING, file);
        FUNCTION_LOG_PARAM(BOOL, ignoreMissing);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(file != NULL);

    FUNCTION_LOG_RETURN(STORAGE_READ, storageReadS3New(this, file, ignoreMissing));
}

/***********************************************************************************************************************************
New file write object
***********************************************************************************************************************************/
static StorageWrite *
storageS3NewWrite(
    THIS_VOID, const String *file, mode_t modeFile, mode_t modePath, const String *user, const String *group, time_t timeModified,
    bool createPath, bool syncFile, bool syncPath, bool atomic)
{
    THIS(StorageS3);

    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STORAGE_S3, this);
        FUNCTION_LOG_PARAM(STRING, file);
        FUNCTION_LOG_PARAM(MODE, modeFile);
        FUNCTION_LOG_PARAM(MODE, modePath);
        FUNCTION_LOG_PARAM(STRING, user);
        FUNCTION_LOG_PARAM(STRING, group);
        FUNCTION_LOG_PARAM(INT64, timeModified);
        FUNCTION_LOG_PARAM(BOOL, createPath);
        FUNCTION_LOG_PARAM(BOOL, syncFile);
        FUNCTION_LOG_PARAM(BOOL, syncPath);
        FUNCTION_LOG_PARAM(BOOL, atomic);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(file != NULL);
    ASSERT(createPath);
    ASSERT(user == NULL);
    ASSERT(group == NULL);
    ASSERT(timeModified == 0);

    FUNCTION_LOG_RETURN(STORAGE_WRITE, storageWriteS3New(this, file, this->partSize));
}

/***********************************************************************************************************************************
Create a path

There are no physical paths on S3 so just return success.
***********************************************************************************************************************************/
static void
storageS3PathCreate(THIS_VOID, const String *path, bool errorOnExists, bool noParentCreate, mode_t mode)
{
    THIS(StorageS3);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_S3, this);
        FUNCTION_LOG_PARAM(STRING, path);
        FUNCTION_LOG_PARAM(BOOL, errorOnExists);
        FUNCTION_LOG_PARAM(BOOL, noParentCreate);
        FUNCTION_LOG_PARAM(MODE, mode);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(path != NULL);
    ASSERT(mode == 0);

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Remove a path
***********************************************************************************************************************************/
static void
storageS3PathRemove(THIS_VOID, const String *path, bool errorOnMissing, bool recurse)
{
    THIS(StorageS3);

    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STORAGE_S3, this);
        FUNCTION_LOG_PARAM(STRING, path);
        FUNCTION_LOG_PARAM(BOOL, errorOnMissing);
        FUNCTION_LOG_PARAM(BOOL, recurse);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(path != NULL);
    ASSERT(!errorOnMissing);

    // S3 doesn't have paths that need to be deleted so nothing to do unless recursing
    if (recurse)
    {
        MEM_CONTEXT_TEMP_BEGIN()
        {
            const String *continuationToken = NULL;

            // Build the base prefix by stripping off the initial /
            const String *basePrefix;

            if (strSize(path) == 1)
                basePrefix = EMPTY_STR;
            else
                basePrefix = strNewFmt("%s/", strPtr(strSub(path, 1)));

            // Loop as long as a continuation token returned
            do
            {
                // Use an inner mem context here because we could potentially be retrieving millions of files so it is a good idea
                // to free memory at regular intervals
                MEM_CONTEXT_TEMP_BEGIN()
                {
                    HttpQuery *query = httpQueryNew();

                    // Add continuation token from the prior loop if any
                    if (continuationToken != NULL)
                        httpQueryAdd(query, S3_QUERY_CONTINUATION_TOKEN_STR, continuationToken);

                    // Use list type 2
                    httpQueryAdd(query, S3_QUERY_LIST_TYPE_STR, S3_QUERY_VALUE_LIST_TYPE_2_STR);

                    // Don't specified empty prefix because it is the default
                    if (!strEmpty(basePrefix))
                        httpQueryAdd(query, S3_QUERY_PREFIX_STR, basePrefix);

                    XmlNode *xmlRoot = xmlDocumentRoot(
                        xmlDocumentNewBuf(
                            storageS3Request(this, HTTP_VERB_GET_STR, FSLASH_STR, query, NULL, true, false).response));

                    // Get file list to delete
                    XmlNodeList *fileList = xmlNodeChildList(xmlRoot, S3_XML_TAG_CONTENTS_STR);
                    XmlDocument *delete = NULL;

                    for (unsigned int fileIdx = 0; fileIdx < xmlNodeLstSize(fileList); fileIdx++)
                    {
                        // If there is something to delete then create the request
                        if (delete == NULL)
                        {
                            delete = xmlDocumentNew(S3_XML_TAG_DELETE_STR);
                            xmlNodeContentSet(xmlNodeAdd(xmlDocumentRoot(delete), S3_XML_TAG_QUIET_STR), TRUE_STR);
                        }

                        // Add to delete list
                        xmlNodeContentSet(
                            xmlNodeAdd(xmlNodeAdd(xmlDocumentRoot(delete), S3_XML_TAG_OBJECT_STR), S3_XML_TAG_KEY_STR),
                            xmlNodeContent(xmlNodeChild(xmlNodeLstGet(fileList, fileIdx), S3_XML_TAG_KEY_STR, true)));
                    }

                    // If there is something to delete then send the request
                    if (delete != NULL)
                    {
                        // Delete file list
                        Buffer *xml = storageS3Request(
                            this, HTTP_VERB_POST_STR, FSLASH_STR, httpQueryAdd(httpQueryNew(), S3_QUERY_DELETE_STR, EMPTY_STR),
                            xmlDocumentBuf(delete), true, false).response;

                        // Nothing is returned when there are no errors
                        if (xml != NULL)
                        {
                            XmlNodeList *errorList = xmlNodeChildList(
                                xmlDocumentRoot(xmlDocumentNewBuf(xml)), S3_XML_TAG_ERROR_STR);

                            if (xmlNodeLstSize(errorList) > 0)
                            {
                                XmlNode *error = xmlNodeLstGet(errorList, 0);

                                THROW_FMT(
                                    FileRemoveError, "unable to remove '%s': [%s] %s",
                                    strPtr(xmlNodeContent(xmlNodeChild(error, S3_XML_TAG_KEY_STR, true))),
                                    strPtr(xmlNodeContent(xmlNodeChild(error, S3_XML_TAG_CODE_STR, true))),
                                    strPtr(xmlNodeContent(xmlNodeChild(error, S3_XML_TAG_MESSAGE_STR, true))));
                            }
                        }
                    }

                    // Get the continuation token and store it in the outer temp context
                    memContextSwitch(MEM_CONTEXT_OLD());
                    continuationToken = xmlNodeContent(xmlNodeChild(xmlRoot, S3_XML_TAG_NEXT_CONTINUATION_TOKEN_STR, false));
                    memContextSwitch(MEM_CONTEXT_TEMP());
                }
                MEM_CONTEXT_TEMP_END();
            }
            while (continuationToken != NULL);
        }
        MEM_CONTEXT_TEMP_END();
    }

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Sync a path

There's no need for this on S3 so just return success.
***********************************************************************************************************************************/
static void
storageS3PathSync(THIS_VOID, const String *path, bool ignoreMissing)
{
    THIS(StorageS3);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_S3, this);
        FUNCTION_LOG_PARAM(STRING, path);
        FUNCTION_LOG_PARAM(BOOL, ignoreMissing);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(path != NULL);

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Remove a file
***********************************************************************************************************************************/
static void
storageS3Remove(THIS_VOID, const String *file, bool errorOnMissing)
{
    THIS(StorageS3);

    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STORAGE_S3, this);
        FUNCTION_LOG_PARAM(STRING, file);
        FUNCTION_LOG_PARAM(BOOL, errorOnMissing);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(file != NULL);
    ASSERT(!errorOnMissing);

    storageS3Request(this, HTTP_VERB_DELETE_STR, file, NULL, NULL, false, false);

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Get http client
***********************************************************************************************************************************/
HttpClient *
storageS3HttpClient(const StorageS3 *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_LOG_PARAM(STORAGE_S3, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->httpClient);
}

/***********************************************************************************************************************************
New object
***********************************************************************************************************************************/
Storage *
storageS3New(
    const String *path, bool write, StoragePathExpressionCallback pathExpressionFunction, const String *bucket,
    const String *endPoint, const String *region, const String *accessKey, const String *secretAccessKey,
    const String *securityToken, size_t partSize, const String *host, unsigned int port, TimeMSec timeout, bool verifyPeer,
    const String *caFile, const String *caPath)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STRING, path);
        FUNCTION_LOG_PARAM(BOOL, write);
        FUNCTION_LOG_PARAM(FUNCTIONP, pathExpressionFunction);
        FUNCTION_LOG_PARAM(STRING, bucket);
        FUNCTION_LOG_PARAM(STRING, endPoint);
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

    Storage *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("StorageS3")
    {
        StorageS3 *driver = memNew(sizeof(StorageS3));
        driver->memContext = MEM_CONTEXT_NEW();

        driver->bucket = strDup(bucket);
        driver->region = strDup(region);
        driver->accessKey = strDup(accessKey);
        driver->secretAccessKey = strDup(secretAccessKey);
        driver->securityToken = strDup(securityToken);
        driver->partSize = partSize;
        driver->host = host == NULL ? strNewFmt("%s.%s", strPtr(bucket), strPtr(endPoint)) : strDup(host);
        driver->port = port;

        // Force the signing key to be generated on the first run
        driver->signingKeyDate = YYYYMMDD_STR;

        // Create the http client used to service requests
        driver->httpClient = httpClientNew(driver->host, driver->port, timeout, verifyPeer, caFile, caPath);
        driver->headerRedactList = strLstAdd(strLstNew(), S3_HEADER_AUTHORIZATION_STR);

        this = storageNewP(
            STORAGE_S3_TYPE_STR, path, 0, 0, write, pathExpressionFunction, driver,
            .exists = storageS3Exists, .info = storageS3Info, .list = storageS3List, .newRead = storageS3NewRead,
            .newWrite = storageS3NewWrite, .pathCreate = storageS3PathCreate, .pathRemove = storageS3PathRemove,
            .pathSync = storageS3PathSync, .remove = storageS3Remove);
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_LOG_RETURN(STORAGE, this);
}
