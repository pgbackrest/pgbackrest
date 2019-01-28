/***********************************************************************************************************************************
S3 Storage Driver
***********************************************************************************************************************************/
#include <time.h>

#include "common/debug.h"
#include "common/io/http/common.h"
#include "common/log.h"
#include "common/memContext.h"
#include "common/regExp.h"
#include "common/type/xml.h"
#include "crypto/hash.h"
#include "storage/driver/s3/fileRead.h"
#include "storage/driver/s3/storage.h"

/***********************************************************************************************************************************
Driver type constant string
***********************************************************************************************************************************/
STRING_EXTERN(STORAGE_DRIVER_S3_TYPE_STR,                           STORAGE_DRIVER_S3_TYPE);

/***********************************************************************************************************************************
S3 http headers
***********************************************************************************************************************************/
STRING_STATIC(S3_HEADER_AUTHORIZATION_STR,                          "authorization");
STRING_STATIC(S3_HEADER_CONTENT_SHA256_STR,                         "x-amz-content-sha256");
STRING_STATIC(S3_HEADER_DATE_STR,                                   "x-amz-date");
STRING_STATIC(S3_HEADER_HOST_STR,                                   "host");
STRING_STATIC(S3_HEADER_TOKEN_STR,                                  "x-amz-security-token");

/***********************************************************************************************************************************
S3 query tokens
***********************************************************************************************************************************/
STRING_STATIC(S3_QUERY_CONTINUATION_TOKEN_STR,                      "continuation-token");
STRING_STATIC(S3_QUERY_DELIMITER_STR,                               "delimiter");
STRING_STATIC(S3_QUERY_LIST_TYPE_STR,                               "list-type");
STRING_STATIC(S3_QUERY_PREFIX_STR,                                  "prefix");

STRING_STATIC(S3_QUERY_VALUE_LIST_TYPE_2_STR,                       "2");

/***********************************************************************************************************************************
XML tags
***********************************************************************************************************************************/
STRING_STATIC(S3_XML_TAG_COMMON_PREFIXES_STR,                       "CommonPrefixes");
STRING_STATIC(S3_XML_TAG_CONTENTS_STR,                              "Contents");
STRING_STATIC(S3_XML_TAG_KEY_STR,                                   "Key");
STRING_STATIC(S3_XML_TAG_NEXT_CONTINUATION_TOKEN_STR,               "NextContinuationToken");
STRING_STATIC(S3_XML_TAG_PREFIX_STR,                                "Prefix");

/***********************************************************************************************************************************
AWS authentication v4 constants
***********************************************************************************************************************************/
#define S3                                                          "s3"
#define AWS4                                                        "AWS4"
#define AWS4_REQUEST                                                "aws4_request"
#define AWS4_HMAC_SHA256                                            "AWS4-HMAC-SHA256"

/***********************************************************************************************************************************
Starting data for signing string so it will be regenerated on the first request
***********************************************************************************************************************************/
STRING_STATIC(YYYYMMDD_STR,                                         "YYYYMMDD");

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct StorageDriverS3
{
    MemContext *memContext;
    Storage *interface;                                             // Driver interface
    HttpClient *httpClient;                                         // Http client to service requests
    const StringList *headerRedactList;                             // List of headers to redact from logging

    const String *bucket;                                           // Bucket to store data in
    const String *region;                                           // e.g. us-east-1
    const String *accessKey;                                        // Access key
    const String *secretAccessKey;                                  // Secret access key
    const String *securityToken;                                    // Security token, if any
    const String *host;                                             // Defaults to {bucket}.{endpoint}

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
storageDriverS3DateTime(time_t authTime)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(INT64, authTime);
    FUNCTION_TEST_END();

    char buffer[ISO_8601_DATE_TIME_SIZE + 1];

    if (strftime(                                                                   // {uncoverable - nothing invalid can be passed}
            buffer, sizeof(buffer), "%Y%m%dT%H%M%SZ", gmtime(&authTime)) != ISO_8601_DATE_TIME_SIZE)
        THROW_SYS_ERROR(AssertError, "unable to format date");                      // {+uncoverable}

    FUNCTION_TEST_RETURN(strNew(buffer));
}

/***********************************************************************************************************************************
Generate authorization header and add it to the supplied header list

Based on the excellent documentation at http://docs.aws.amazon.com/AmazonS3/latest/API/sig-v4-header-based-auth.html.
***********************************************************************************************************************************/
static void
storageDriverS3Auth(
    StorageDriverS3 *this, const String *verb, const String *uri, const HttpQuery *query, const String *dateTime,
    HttpHeader *httpHeader, const String *payloadHash)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_DRIVER_S3, this);
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
            strPtr(bufHex(cryptoHashOneStr(HASH_TYPE_SHA256_STR, canonicalRequest))));

        // Generate signing key.  This key only needs to be regenerated every seven days but we'll do it once a day to keep the
        // logic simple.  It's a relatively expensive operation so we'd rather not do it for every request.
        // If the cached signing key has expired (or has noe been generated) then regenerate it
        if (!strEq(date, this->signingKeyDate))
        {
            const Buffer *dateKey = cryptoHmacOne(
                HASH_TYPE_SHA256_STR, bufNewStr(strNewFmt(AWS4 "%s", strPtr(this->secretAccessKey))), bufNewStr(date));
            const Buffer *regionKey = cryptoHmacOne(HASH_TYPE_SHA256_STR, dateKey, bufNewStr(this->region));
            const Buffer *serviceKey = cryptoHmacOne(HASH_TYPE_SHA256_STR, regionKey, bufNewZ(S3));

            // Switch to the object context so signing key and date are not lost
            MEM_CONTEXT_BEGIN(this->memContext)
            {
                this->signingKey = cryptoHmacOne(HASH_TYPE_SHA256_STR, serviceKey, bufNewZ(AWS4_REQUEST));
                this->signingKeyDate = strDup(date);
            }
            MEM_CONTEXT_END();
        }

        // Generate authorization header
        const String *authorization = strNewFmt(
            AWS4_HMAC_SHA256 " Credential=%s/%s/%s/" S3 "/" AWS4_REQUEST ",SignedHeaders=%s,Signature=%s",
            strPtr(this->accessKey), strPtr(date), strPtr(this->region), strPtr(signedHeaders),
            strPtr(bufHex(cryptoHmacOne(HASH_TYPE_SHA256_STR, this->signingKey, bufNewStr(stringToSign)))));

        httpHeaderPut(httpHeader, S3_HEADER_AUTHORIZATION_STR, authorization);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_TEST_RETURN_VOID();
}

/***********************************************************************************************************************************
New object
***********************************************************************************************************************************/
StorageDriverS3 *
storageDriverS3New(
    const String *path, bool write, StoragePathExpressionCallback pathExpressionFunction, const String *bucket,
    const String *endPoint, const String *region, const String *accessKey, const String *secretAccessKey,
    const String *securityToken, const String *host, unsigned int port, TimeMSec timeout, bool verifyPeer, const String *caFile,
    const String *caPath)
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

    // Create the object
    StorageDriverS3 *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("StorageDriverS3")
    {
        this = memNew(sizeof(StorageDriverS3));
        this->memContext = MEM_CONTEXT_NEW();

        this->bucket = strDup(bucket);
        this->region = strDup(region);
        this->accessKey = strDup(accessKey);
        this->secretAccessKey = strDup(secretAccessKey);
        this->securityToken = strDup(securityToken);
        this->host = host == NULL ? strNewFmt("%s.%s", strPtr(bucket), strPtr(endPoint)) : strDup(host);

        // Force the signing key to be generated on the first run
        this->signingKeyDate = YYYYMMDD_STR;

        // Create the storage interface
        this->interface = storageNewP(
            STORAGE_DRIVER_S3_TYPE_STR, path, 0, 0, write, true, pathExpressionFunction, this,
            .exists = (StorageInterfaceExists)storageDriverS3Exists, .info = (StorageInterfaceInfo)storageDriverS3Info,
            .list = (StorageInterfaceList)storageDriverS3List, .newRead = (StorageInterfaceNewRead)storageDriverS3NewRead,
            .newWrite = (StorageInterfaceNewWrite)storageDriverS3NewWrite,
            .pathCreate = (StorageInterfacePathCreate)storageDriverS3PathCreate,
            .pathRemove = (StorageInterfacePathRemove)storageDriverS3PathRemove,
            .pathSync = (StorageInterfacePathSync)storageDriverS3PathSync, .remove = (StorageInterfaceRemove)storageDriverS3Remove);

        // Create the http client used to service requests
        this->httpClient = httpClientNew(this->host, port, timeout, verifyPeer, caFile, caPath);
        this->headerRedactList = strLstAdd(strLstNew(), S3_HEADER_AUTHORIZATION_STR);
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_LOG_RETURN(STORAGE_DRIVER_S3, this);
}

/***********************************************************************************************************************************
Process S3 request
***********************************************************************************************************************************/
Buffer *
storageDriverS3Request(
    StorageDriverS3 *this, const String *verb, const String *uri, const HttpQuery *query, const Buffer *body, bool returnContent,
    bool allowMissing)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STORAGE_DRIVER_S3, this);
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

    Buffer *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Create header list and add content length
        HttpHeader *requestHeader = httpHeaderNew(this->headerRedactList);
        httpHeaderAdd(requestHeader, HTTP_HEADER_CONTENT_LENGTH_STR, ZERO_STR);

        // Generate authorization header
        storageDriverS3Auth(
            this, verb, uri, query, storageDriverS3DateTime(time(NULL)), requestHeader,
            strNew("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"));

        // Process request
        result = httpClientRequest(this->httpClient, verb, uri, query, requestHeader, returnContent);

        // Error if the request was not successful
        if (httpClientResponseCode(this->httpClient) != HTTP_RESPONSE_CODE_OK &&
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
            if (result != NULL)
                strCatFmt(error, "\n*** Response Content ***:\n%s", strPtr(strNewBuf(result)));

            THROW(ProtocolError, strPtr(error));
        }

        // On success move the buffer to the calling context
        bufMove(result, MEM_CONTEXT_OLD());
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(BUFFER, result);
}

/***********************************************************************************************************************************
Does a file/path exist?
***********************************************************************************************************************************/
bool
storageDriverS3Exists(StorageDriverS3 *this, const String *path)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STORAGE_DRIVER_S3, this);
        FUNCTION_LOG_PARAM(STRING, path);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(path != NULL);

    bool result = false;

    THROW(AssertError, "NOT YET IMPLEMENTED");

    FUNCTION_LOG_RETURN(BOOL, result);
}

/***********************************************************************************************************************************
File/path info
***********************************************************************************************************************************/
StorageInfo
storageDriverS3Info(StorageDriverS3 *this, const String *file, bool ignoreMissing)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STORAGE_DRIVER_S3, this);
        FUNCTION_LOG_PARAM(STRING, file);
        FUNCTION_LOG_PARAM(BOOL, ignoreMissing);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(file != NULL);

    THROW(AssertError, "NOT YET IMPLEMENTED");

    FUNCTION_LOG_RETURN(STORAGE_INFO, (StorageInfo){0});
}

/***********************************************************************************************************************************
Get a list of files from a directory
***********************************************************************************************************************************/
StringList *
storageDriverS3List(StorageDriverS3 *this, const String *path, bool errorOnMissing, const String *expression)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STORAGE_DRIVER_S3, this);
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
        RegExp *regExp = (expression == NULL) ? NULL : regExpNew(expression);

        // Build the base prefix by stripping of the initial /
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
                    xmlDocumentNewBuf(storageDriverS3Request(this, HTTP_VERB_GET_STR, FSLASH_STR, query, NULL, true, false)));

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
StorageFileRead *
storageDriverS3NewRead(StorageDriverS3 *this, const String *file, bool ignoreMissing)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STORAGE_DRIVER_S3, this);
        FUNCTION_LOG_PARAM(STRING, file);
        FUNCTION_LOG_PARAM(BOOL, ignoreMissing);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(file != NULL);

    FUNCTION_LOG_RETURN(
        STORAGE_FILE_READ, storageDriverS3FileReadInterface(storageDriverS3FileReadNew(this, file, ignoreMissing)));
}

/***********************************************************************************************************************************
New file write object
***********************************************************************************************************************************/
StorageFileWrite *
storageDriverS3NewWrite(
    StorageDriverS3 *this, const String *file, mode_t modeFile, mode_t modePath, bool createPath, bool syncFile,
    bool syncPath, bool atomic)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STORAGE_DRIVER_S3, this);
        FUNCTION_LOG_PARAM(STRING, file);
        FUNCTION_LOG_PARAM(MODE, modeFile);
        FUNCTION_LOG_PARAM(MODE, modePath);
        FUNCTION_LOG_PARAM(BOOL, createPath);
        FUNCTION_LOG_PARAM(BOOL, syncFile);
        FUNCTION_LOG_PARAM(BOOL, syncPath);
        FUNCTION_LOG_PARAM(BOOL, atomic);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(file != NULL);
    ASSERT(modeFile == 0);
    ASSERT(modePath == 0);

    THROW(AssertError, "NOT YET IMPLEMENTED");

    FUNCTION_LOG_RETURN(STORAGE_FILE_WRITE, NULL);
}

/***********************************************************************************************************************************
Create a path.  There are no physical paths on S3 so just return success.
***********************************************************************************************************************************/
void
storageDriverS3PathCreate(StorageDriverS3 *this, const String *path, bool errorOnExists, bool noParentCreate, mode_t mode)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_DRIVER_S3, this);
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
void
storageDriverS3PathRemove(StorageDriverS3 *this, const String *path, bool errorOnMissing, bool recurse)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STORAGE_DRIVER_S3, this);
        FUNCTION_LOG_PARAM(STRING, path);
        FUNCTION_LOG_PARAM(BOOL, errorOnMissing);
        FUNCTION_LOG_PARAM(BOOL, recurse);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(path != NULL);

    THROW(AssertError, "NOT YET IMPLEMENTED");

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Sync a path.  There's no need for this on S3 so just return success.
***********************************************************************************************************************************/
void
storageDriverS3PathSync(StorageDriverS3 *this, const String *path, bool ignoreMissing)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_DRIVER_S3, this);
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
void
storageDriverS3Remove(StorageDriverS3 *this, const String *file, bool errorOnMissing)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STORAGE_DRIVER_S3, this);
        FUNCTION_LOG_PARAM(STRING, file);
        FUNCTION_LOG_PARAM(BOOL, errorOnMissing);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(file != NULL);

    THROW(AssertError, "NOT YET IMPLEMENTED");

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Get http client
***********************************************************************************************************************************/
HttpClient *
storageDriverS3HttpClient(const StorageDriverS3 *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_LOG_PARAM(STORAGE_DRIVER_S3, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->httpClient);
}

/***********************************************************************************************************************************
Get storage interface
***********************************************************************************************************************************/
Storage *
storageDriverS3Interface(const StorageDriverS3 *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_LOG_PARAM(STORAGE_DRIVER_S3, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->interface);
}
