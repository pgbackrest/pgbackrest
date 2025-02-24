/***********************************************************************************************************************************
GCS Storage
***********************************************************************************************************************************/
#include "build.auto.h"

#include <string.h>

#include <openssl/bio.h>
#include <openssl/pem.h>
#include <openssl/x509.h>

#include "common/crypto/common.h"
#include "common/crypto/hash.h"
#include "common/debug.h"
#include "common/io/http/client.h"
#include "common/io/http/common.h"
#include "common/io/http/url.h"
#include "common/io/socket/client.h"
#include "common/io/tls/client.h"
#include "common/log.h"
#include "common/regExp.h"
#include "common/stat.h"
#include "common/type/json.h"
#include "common/type/object.h"
#include "storage/gcs/read.h"
#include "storage/gcs/write.h"
#include "storage/posix/storage.h"

/***********************************************************************************************************************************
Defaults
***********************************************************************************************************************************/
#define STORAGE_GCS_DELETE_MAX                                      100

/***********************************************************************************************************************************
HTTP headers
***********************************************************************************************************************************/
STRING_EXTERN(GCS_HEADER_UPLOAD_ID_STR,                             GCS_HEADER_UPLOAD_ID);
STRING_STATIC(GCS_HEADER_METADATA_FLAVOR_STR,                       "metadata-flavor");
STRING_STATIC(GCS_HEADER_GOOGLE_STR,                                "Google");

/***********************************************************************************************************************************
Query tokens
***********************************************************************************************************************************/
STRING_STATIC(GCS_QUERY_DELIMITER_STR,                              "delimiter");
STRING_EXTERN(GCS_QUERY_FIELDS_STR,                                 GCS_QUERY_FIELDS);
STRING_EXTERN(GCS_QUERY_MEDIA_STR,                                  GCS_QUERY_MEDIA);
STRING_EXTERN(GCS_QUERY_NAME_STR,                                   GCS_QUERY_NAME);
STRING_STATIC(GCS_QUERY_PAGE_TOKEN_STR,                             "pageToken");
STRING_STATIC(GCS_QUERY_PREFIX_STR,                                 "prefix");
STRING_EXTERN(GCS_QUERY_UPLOAD_ID_STR,                              GCS_QUERY_UPLOAD_ID);
STRING_STATIC(GCS_QUERY_VERSIONS_STR,                               "versions");
STRING_STATIC(GCS_QUERY_USER_PROJECT_STR,                           "userProject");

/***********************************************************************************************************************************
JSON tokens
***********************************************************************************************************************************/
VARIANT_STRDEF_STATIC(GCS_JSON_ACCESS_TOKEN_VAR,                    "access_token");
VARIANT_STRDEF_STATIC(GCS_JSON_CLIENT_EMAIL_VAR,                    "client_email");
VARIANT_STRDEF_STATIC(GCS_JSON_ERROR_VAR,                           "error");
VARIANT_STRDEF_STATIC(GCS_JSON_ERROR_DESCRIPTION_VAR,               "error_description");
VARIANT_STRDEF_STATIC(GCS_JSON_EXPIRES_IN_VAR,                      "expires_in");
#define GCS_JSON_ITEMS                                              "items"
VARIANT_STRDEF_STATIC(GCS_JSON_ITEMS_VAR,                           GCS_JSON_ITEMS);
VARIANT_STRDEF_EXTERN(GCS_JSON_GENERATION_VAR,                      GCS_JSON_GENERATION);
VARIANT_STRDEF_EXTERN(GCS_JSON_MD5_HASH_VAR,                        GCS_JSON_MD5_HASH);
VARIANT_STRDEF_EXTERN(GCS_JSON_NAME_VAR,                            GCS_JSON_NAME);
#define GCS_JSON_NEXT_PAGE_TOKEN                                    "nextPageToken"
VARIANT_STRDEF_STATIC(GCS_JSON_NEXT_PAGE_TOKEN_VAR,                 GCS_JSON_NEXT_PAGE_TOKEN);
#define GCS_JSON_PREFIXES                                           "prefixes"
VARIANT_STRDEF_STATIC(GCS_JSON_PREFIXES_VAR,                        GCS_JSON_PREFIXES);
VARIANT_STRDEF_STATIC(GCS_JSON_PRIVATE_KEY_VAR,                     "private_key");
VARIANT_STRDEF_EXTERN(GCS_JSON_SIZE_VAR,                            GCS_JSON_SIZE);
VARIANT_STRDEF_STATIC(GCS_JSON_TOKEN_TYPE_VAR,                      "token_type");
VARIANT_STRDEF_STATIC(GCS_JSON_TOKEN_URI_VAR,                       "token_uri");
#define GCS_JSON_UPDATED                                            "updated"
VARIANT_STRDEF_STATIC(GCS_JSON_UPDATED_VAR,                         GCS_JSON_UPDATED);

/***********************************************************************************************************************************
Statistics constants
***********************************************************************************************************************************/
STRING_STATIC(GCS_STAT_REMOVE_STR,                                  "gcs.rm");
STRING_STATIC(GCS_STAT_REMOVE_BATCH_STR,                            "gcs.rm.batch");
STRING_STATIC(GCS_STAT_REMOVE_BATCH_PART_STR,                       "gcs.rm.batch.part");
STRING_STATIC(GCS_STAT_REMOVE_BATCH_RETRY_STR,                      "gcs.rm.batch.retry");

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct StorageGcs
{
    STORAGE_COMMON_MEMBER;
    HttpClient *httpClient;                                         // Http client to service requests
    StringList *headerRedactList;                                   // List of headers to redact from logging
    StringList *queryRedactList;                                    // List of query keys to redact from logging

    bool write;                                                     // Storage is writable
    const String *bucket;                                           // Bucket to store data in
    const String *endpoint;                                         // Endpoint
    size_t chunkSize;                                               // Block size for resumable upload
    unsigned int deleteMax;                                         // Maximum objects that can be deleted in one request
    const Buffer *tag;                                              // Tags to be applied to objects
    const String *userProject;                                      // Project ID

    StorageGcsKeyType keyType;                                      // Auth key type
    const String *key;                                              // Key (value depends on key type)
    String *token;                                                  // Token
    time_t tokenTimeExpire;                                         // Token expiration time (if service auth)
    HttpUrl *authUrl;                                               // URL for authentication server
    HttpClient *authClient;                                         // Client to service auth requests
};

/***********************************************************************************************************************************
Parse HTTP JSON response containing an authentication token and expiration

Note that the function is intended to run directly in the caller's mem context and results will be placed in the caller's prior mem
context.
***********************************************************************************************************************************/
typedef struct
{
    String *tokenType;
    String *token;
    time_t timeExpire;
} StorageGcsAuthTokenResult;

static StorageGcsAuthTokenResult
storageGcsAuthToken(HttpRequest *const request, const time_t timeBegin)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(HTTP_REQUEST, request);
        FUNCTION_TEST_PARAM(TIME, timeBegin);
    FUNCTION_TEST_END();

    FUNCTION_AUDIT_STRUCT();

    StorageGcsAuthTokenResult result = {0};

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Get the response
        const KeyValue *const kvResponse = varKv(jsonToVar(strNewBuf(httpResponseContent(httpRequestResponse(request, true)))));

        // Check for an error
        const String *const error = varStr(kvGet(kvResponse, GCS_JSON_ERROR_VAR));

        if (error != NULL)
        {
            THROW_FMT(
                ProtocolError, "unable to get authentication token: [%s] %s", strZ(error),
                strZNull(varStr(kvGet(kvResponse, GCS_JSON_ERROR_DESCRIPTION_VAR))));
        }

        MEM_CONTEXT_PRIOR_BEGIN()
        {
            // Get token
            result.tokenType = strDup(varStr(kvGet(kvResponse, GCS_JSON_TOKEN_TYPE_VAR)));
            CHECK(FormatError, result.tokenType != NULL, "token type missing");
            result.token = strDup(varStr(kvGet(kvResponse, GCS_JSON_ACCESS_TOKEN_VAR)));
            CHECK(FormatError, result.token != NULL, "access token missing");

            // Get expiration
            const Variant *const expiresIn = kvGet(kvResponse, GCS_JSON_EXPIRES_IN_VAR);
            CHECK(FormatError, expiresIn != NULL, "expiry missing");

            result.timeExpire = timeBegin + (time_t)varInt64Force(expiresIn);
        }
        MEM_CONTEXT_PRIOR_END();
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_TEST_RETURN_TYPE(StorageGcsAuthTokenResult, result);
}

/***********************************************************************************************************************************
Get authentication header for service keys

Based on the documentation at https://developers.google.com/identity/protocols/oauth2/service-account#httprest
***********************************************************************************************************************************/
// Helper to construct a JSON Web Token
static String *
storageGcsAuthJwt(StorageGcs *const this, const time_t timeBegin)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_GCS, this);
        FUNCTION_TEST_PARAM(TIME, timeBegin);
    FUNCTION_TEST_END();

    // Static header with dot delimiter
    String *const result = strCatZ(strNew(), "eyJhbGciOiJSUzI1NiIsInR5cCI6IkpXVCJ9.");

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Load client email and private key
        const KeyValue *const kvKey = varKv(
            jsonToVar(strNewBuf(storageGetP(storageNewReadP(storagePosixNewP(FSLASH_STR), this->key)))));
        const String *const clientEmail = varStr(kvGet(kvKey, GCS_JSON_CLIENT_EMAIL_VAR));
        const String *const privateKeyRaw = varStr(kvGet(kvKey, GCS_JSON_PRIVATE_KEY_VAR));

        CHECK(FormatError, clientEmail != NULL && privateKeyRaw != NULL, "credentials missing");

        // Add claim
        strCatEncode(
            result, encodingBase64Url,
            BUFSTR(
                strNewFmt(
                    "{\"iss\":\"%s\",\"scope\":\"https://www.googleapis.com/auth/devstorage.read%s\",\"aud\":\"%s\""
                    ",\"exp\":%" PRIu64 ",\"iat\":%" PRIu64 "}",
                    strZ(clientEmail), this->write ? "_write" : "_only", strZ(httpUrl(this->authUrl)),
                    (uint64_t)timeBegin + 3600, (uint64_t)timeBegin)));

        // Sign with RSA key
        BIO *volatile bio = NULL;
        EVP_PKEY *volatile privateKey = NULL;
        EVP_MD_CTX *volatile sign = NULL;

        cryptoInit();

        TRY_BEGIN()
        {
            // Load key
            bio = BIO_new(BIO_s_mem());
            BIO_write(bio, strZ(privateKeyRaw), (int)strSize(privateKeyRaw));

            privateKey = PEM_read_bio_PrivateKey(bio, NULL, NULL, NULL);
            cryptoError(privateKey == NULL, "unable to read PEM");

            // Create signature
            sign = EVP_MD_CTX_create();
            cryptoError(EVP_DigestSignInit(sign, NULL, EVP_sha256(), NULL, privateKey) <= 0, "unable to init");
            cryptoError(
                EVP_DigestSignUpdate(sign, (const unsigned char *)strZ(result), (unsigned int)strSize(result)) <= 0,
                "unable to update");

            size_t signatureLen = 0;
            cryptoError(EVP_DigestSignFinal(sign, NULL, &signatureLen) <= 0, "unable to get size");

            Buffer *const signature = bufNew(signatureLen);
            bufUsedSet(signature, bufSize(signature));

            cryptoError(EVP_DigestSignFinal(sign, bufPtr(signature), &signatureLen) <= 0, "unable to finalize");

            // Add dot delimiter and signature
            strCatChr(result, '.');
            strCatEncode(result, encodingBase64Url, signature);
        }
        FINALLY()
        {
            BIO_free(bio);
            EVP_MD_CTX_free(sign);
            EVP_PKEY_free(privateKey);
        }
        TRY_END();
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_TEST_RETURN(STRING, result);
}

static StorageGcsAuthTokenResult
storageGcsAuthService(StorageGcs *const this, const time_t timeBegin)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_GCS, this);
        FUNCTION_TEST_PARAM(TIME, timeBegin);
    FUNCTION_TEST_END();

    FUNCTION_AUDIT_STRUCT();

    ASSERT(this != NULL);
    ASSERT(timeBegin > 0);

    StorageGcsAuthTokenResult result = {0};

    MEM_CONTEXT_TEMP_BEGIN()
    {
        const String *const content = strNewFmt(
            "grant_type=urn%%3Aietf%%3Aparams%%3Aoauth%%3Agrant-type%%3Ajwt-bearer&assertion=%s",
            strZ(storageGcsAuthJwt(this, timeBegin)));

        HttpHeader *const header = httpHeaderNew(NULL);
        httpHeaderAdd(header, HTTP_HEADER_HOST_STR, httpUrlHost(this->authUrl));
        httpHeaderAdd(header, HTTP_HEADER_CONTENT_TYPE_STR, HTTP_HEADER_CONTENT_TYPE_APP_FORM_URL_STR);
        httpHeaderAdd(header, HTTP_HEADER_CONTENT_LENGTH_STR, strNewFmt("%zu", strSize(content)));

        HttpRequest *const request = httpRequestNewP(
            this->authClient, HTTP_VERB_POST_STR, httpUrlPath(this->authUrl), NULL, .header = header, .content = BUFSTR(content));

        MEM_CONTEXT_PRIOR_BEGIN()
        {
            result = storageGcsAuthToken(request, timeBegin);
        }
        MEM_CONTEXT_PRIOR_END();
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_TEST_RETURN_TYPE(StorageGcsAuthTokenResult, result);
}

/***********************************************************************************************************************************
Get authentication token automatically for instances running in GCE.

Based on the documentation at https://cloud.google.com/compute/docs/access/create-enable-service-accounts-for-instances#applications
***********************************************************************************************************************************/
static StorageGcsAuthTokenResult
storageGcsAuthAuto(StorageGcs *const this, const time_t timeBegin)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_GCS, this);
        FUNCTION_TEST_PARAM(TIME, timeBegin);
    FUNCTION_TEST_END();

    FUNCTION_AUDIT_STRUCT();

    ASSERT(this != NULL);
    ASSERT(timeBegin > 0);

    StorageGcsAuthTokenResult result = {0};

    MEM_CONTEXT_TEMP_BEGIN()
    {
        HttpHeader *const header = httpHeaderNew(NULL);
        httpHeaderAdd(header, HTTP_HEADER_HOST_STR, httpUrlHost(this->authUrl));
        httpHeaderAdd(header, GCS_HEADER_METADATA_FLAVOR_STR, GCS_HEADER_GOOGLE_STR);
        httpHeaderAdd(header, HTTP_HEADER_CONTENT_LENGTH_STR, ZERO_STR);

        HttpRequest *const request = httpRequestNewP(
            this->authClient, HTTP_VERB_GET_STR, httpUrlPath(this->authUrl), NULL, .header = header);

        MEM_CONTEXT_PRIOR_BEGIN()
        {
            result = storageGcsAuthToken(request, timeBegin);
        }
        MEM_CONTEXT_PRIOR_END();
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_TEST_RETURN_TYPE(StorageGcsAuthTokenResult, result);
}

/***********************************************************************************************************************************
Generate authorization header and add it to the supplied header list
***********************************************************************************************************************************/
static void
storageGcsAuth(StorageGcs *const this, HttpHeader *const httpHeader)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_GCS, this);
        FUNCTION_TEST_PARAM(HTTP_HEADER, httpHeader);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(httpHeader != NULL);
    ASSERT(httpHeaderGet(httpHeader, HTTP_HEADER_CONTENT_LENGTH_STR) != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Get the token if it was not supplied by the user
        if (this->keyType != storageGcsKeyTypeToken)
        {
            ASSERT(this->keyType == storageGcsKeyTypeAuto || this->keyType == storageGcsKeyTypeService);

            const time_t timeBegin = time(NULL);

            // If the current token has expired then request a new one
            if (timeBegin >= this->tokenTimeExpire)
            {
                const StorageGcsAuthTokenResult tokenResult =
                    this->keyType == storageGcsKeyTypeAuto ?
                        storageGcsAuthAuto(this, timeBegin) : storageGcsAuthService(this, timeBegin);

                MEM_CONTEXT_OBJ_BEGIN(this)
                {
                    strFree(this->token);
                    this->token = strNewFmt("%s %s", strZ(tokenResult.tokenType), strZ(tokenResult.token));

                    // Subtract http client timeout * 2 so the token does not expire in the middle of http retries
                    this->tokenTimeExpire =
                        tokenResult.timeExpire - ((time_t)(httpClientTimeout(this->httpClient) / MSEC_PER_SEC * 2));
                }
                MEM_CONTEXT_OBJ_END();
            }
        }

        // Add authorization header
        httpHeaderPut(httpHeader, HTTP_HEADER_AUTHORIZATION_STR, this->token);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_TEST_RETURN_VOID();
}

/***********************************************************************************************************************************
Process Gcs request
***********************************************************************************************************************************/
// Helper to generate request path
static String *
storageGcsRequestPath(StorageGcs *const this, const String *const object, const bool bucket, const bool upload)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_GCS, this);
        FUNCTION_TEST_PARAM(STRING, object);
        FUNCTION_TEST_PARAM(BOOL, bucket);
        FUNCTION_TEST_PARAM(BOOL, upload);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    String *const result = strNew();

    MEM_CONTEXT_TEMP_BEGIN()
    {
        strCatFmt(result, "%s/storage/v1/b", upload ? "/upload" : "");

        if (bucket)
            strCatFmt(result, "/%s/o", strZ(this->bucket));

        if (object != NULL)
            strCatFmt(result, "/%s", strZ(httpUriEncode(strSub(object, 1), false)));
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_TEST_RETURN(STRING, result);
}

FN_EXTERN HttpRequest *
storageGcsRequestAsync(StorageGcs *const this, const String *const verb, StorageGcsRequestAsyncParam param)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STORAGE_GCS, this);
        FUNCTION_LOG_PARAM(STRING, verb);
        FUNCTION_LOG_PARAM(BOOL, param.noBucket);
        FUNCTION_LOG_PARAM(BOOL, param.upload);
        FUNCTION_LOG_PARAM(BOOL, param.noAuth);
        FUNCTION_LOG_PARAM(BOOL, param.tag);
        FUNCTION_LOG_PARAM(STRING, param.path);
        FUNCTION_LOG_PARAM(STRING, param.object);
        FUNCTION_LOG_PARAM(HTTP_HEADER, param.header);
        FUNCTION_LOG_PARAM(HTTP_QUERY, param.query);
        FUNCTION_LOG_PARAM(BUFFER, param.content);
        FUNCTION_LOG_PARAM(LIST, param.contentList);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(verb != NULL);
    ASSERT(!param.noBucket || param.object == NULL);

    HttpRequest *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Generate path
        const String *const path =
            param.path != NULL ? param.path : storageGcsRequestPath(this, param.object, !param.noBucket, param.upload);

        // Create header list
        HttpHeader *const requestHeader =
            param.header == NULL ? httpHeaderNew(this->headerRedactList) : httpHeaderDup(param.header, this->headerRedactList);

        // Add tags
        if (param.tag)
        {
            ASSERT(param.content == NULL);
            ASSERT(this->tag != NULL);

            httpHeaderPut(requestHeader, HTTP_HEADER_CONTENT_TYPE_STR, HTTP_HEADER_CONTENT_TYPE_JSON_STR);
            param.content = this->tag;
        }

        // Set host
        httpHeaderPut(requestHeader, HTTP_HEADER_HOST_STR, this->endpoint);

        // Set content or construct multipart content
        const Buffer *content = param.content;

        if (param.contentList != NULL)
        {
            ASSERT(param.content == NULL);

            HttpRequestMulti *const requestMulti = httpRequestMultiNew();

            for (unsigned int contentIdx = 0; contentIdx < lstSize(param.contentList); contentIdx++)
            {
                const StorageGcsRequestPart *const requestPart = lstGet(param.contentList, contentIdx);
                HttpHeader *const partHeader = httpHeaderNew(this->headerRedactList);

                httpHeaderAdd(partHeader, HTTP_HEADER_CONTENT_LENGTH_STR, ZERO_STR);
                httpRequestMultiAddP(
                    requestMulti, strNewFmt("%u", contentIdx), requestPart->verb,
                    storageGcsRequestPath(this, requestPart->object, true, false), .header = partHeader);
            }

            httpRequestMultiHeaderAdd(requestMulti, requestHeader);
            content = httpRequestMultiContent(requestMulti);
        }

        // Set content length
        httpHeaderPut(
            requestHeader, HTTP_HEADER_CONTENT_LENGTH_STR,
            content == NULL || bufEmpty(content) ? ZERO_STR : strNewFmt("%zu", bufUsed(content)));

        // Make a copy of the query so it can be modified
        HttpQuery *const query =
            this->userProject != NULL && param.query == NULL ?
                httpQueryNewP(.redactList = this->queryRedactList) :
                httpQueryDupP(param.query, .redactList = this->queryRedactList);

        // Add user project
        if (this->userProject != NULL)
            httpQueryAdd(query, GCS_QUERY_USER_PROJECT_STR, this->userProject);

        // Generate authorization header
        if (!param.noAuth)
            storageGcsAuth(this, requestHeader);

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
storageGcsResponse(HttpRequest *const request, const StorageGcsResponseParam param)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(HTTP_REQUEST, request);
        FUNCTION_LOG_PARAM(BOOL, param.allowMissing);
        FUNCTION_LOG_PARAM(BOOL, param.allowIncomplete);
        FUNCTION_LOG_PARAM(BOOL, param.contentIo);
    FUNCTION_LOG_END();

    ASSERT(request != NULL);
    ASSERT(!param.allowMissing || !param.allowIncomplete);

    HttpResponse *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Get response
        result = httpRequestResponse(request, !param.contentIo);

        // Error if the request was not successful
        if (!httpResponseCodeOk(result) && (!param.allowMissing || httpResponseCode(result) != HTTP_RESPONSE_CODE_NOT_FOUND) &&
            (!param.allowIncomplete || httpResponseCode(result) != HTTP_RESPONSE_CODE_PERMANENT_REDIRECT))
            httpRequestError(request, result);

        // Move response to the prior context
        httpResponseMove(result, memContextPrior());
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(HTTP_RESPONSE, result);
}

FN_EXTERN HttpResponse *
storageGcsRequest(StorageGcs *const this, const String *const verb, const StorageGcsRequestParam param)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STORAGE_GCS, this);
        FUNCTION_LOG_PARAM(STRING, verb);
        FUNCTION_LOG_PARAM(BOOL, param.noBucket);
        FUNCTION_LOG_PARAM(BOOL, param.upload);
        FUNCTION_LOG_PARAM(BOOL, param.noAuth);
        FUNCTION_LOG_PARAM(BOOL, param.tag);
        FUNCTION_LOG_PARAM(STRING, param.path);
        FUNCTION_LOG_PARAM(STRING, param.object);
        FUNCTION_LOG_PARAM(HTTP_HEADER, param.header);
        FUNCTION_LOG_PARAM(HTTP_QUERY, param.query);
        FUNCTION_LOG_PARAM(BUFFER, param.content);
        FUNCTION_LOG_PARAM(BOOL, param.allowMissing);
        FUNCTION_LOG_PARAM(BOOL, param.allowIncomplete);
        FUNCTION_LOG_PARAM(BOOL, param.contentIo);
    FUNCTION_LOG_END();

    HttpRequest *const request = storageGcsRequestAsyncP(
        this, verb, .noBucket = param.noBucket, .upload = param.upload, .noAuth = param.noAuth, .tag = param.tag,
        .path = param.path, .object = param.object, .header = param.header, .query = param.query, .content = param.content,
        .contentList = param.contentList);
    HttpResponse *const result = storageGcsResponseP(
        request, .allowMissing = param.allowMissing, .allowIncomplete = param.allowIncomplete, .contentIo = param.contentIo);

    httpRequestFree(request);

    FUNCTION_LOG_RETURN(HTTP_RESPONSE, result);
}

/***********************************************************************************************************************************
General function for listing files to be used by other list routines

Based on the documentation at https://cloud.google.com/storage/docs/json_api/v1/objects/list
***********************************************************************************************************************************/
// Helper to convert YYYY-MM-DDTHH:MM:SS.MSECZ format to time_t. This format is very nearly ISO-8601 except for the inclusion of
// milliseconds, which are discarded here.
static time_t
storageGcsCvtTime(const String *const time)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, time);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RETURN(
        TIME,
        epochFromParts(
            cvtZSubNToInt(strZ(time), 0, 4), cvtZSubNToInt(strZ(time), 5, 2), cvtZSubNToInt(strZ(time), 8, 2),
            cvtZSubNToInt(strZ(time), 11, 2), cvtZSubNToInt(strZ(time), 14, 2), cvtZSubNToInt(strZ(time), 17, 2), 0));
}

static void
storageGcsInfoFile(StorageInfo *const info, const KeyValue *const file)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_INFO, info);
        FUNCTION_TEST_PARAM(KEY_VALUE, file);
    FUNCTION_TEST_END();

    info->size = cvtZToUInt64(strZ(varStr(kvGet(file, GCS_JSON_SIZE_VAR))));
    info->timeModified = storageGcsCvtTime(varStr(kvGet(file, GCS_JSON_UPDATED_VAR)));

    FUNCTION_TEST_RETURN_VOID();
}

static void
storageGcsListInternal(
    StorageGcs *const this, const String *const path, const StorageInfoLevel level, const String *const expression,
    const bool recurse, const time_t targetTime, StorageListCallback callback, void *const callbackData)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STORAGE_GCS, this);
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
            httpQueryAdd(query, GCS_QUERY_DELIMITER_STR, FSLASH_STR);

        // Don't specify empty prefix because it is the default
        if (!strEmpty(queryPrefix))
            httpQueryAdd(query, GCS_QUERY_PREFIX_STR, queryPrefix);

        // Specify fields to limit the amount of data returned
        String *const fields = strCatZ(
            strNew(), GCS_JSON_NEXT_PAGE_TOKEN "," GCS_JSON_PREFIXES "," GCS_JSON_ITEMS "(" GCS_JSON_NAME);

        if (level >= storageInfoLevelBasic || targetTime != 0)
        {
            if (level >= storageInfoLevelBasic)
                strCatZ(fields, "," GCS_JSON_SIZE);

            strCatZ(fields, "," GCS_JSON_UPDATED);

            if (targetTime != 0)
                strCatZ(fields, "," GCS_JSON_GENERATION);
        }

        strCatChr(fields, ')');
        httpQueryAdd(query, GCS_QUERY_FIELDS_STR, fields);

        // Add versions
        if (targetTime != 0)
            httpQueryAdd(query, GCS_QUERY_VERSIONS_STR, TRUE_STR);

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
                    response = storageGcsResponseP(request);

                    httpRequestFree(request);
                    request = NULL;
                }
                // Else get the response immediately from a sync request
                else
                    response = storageGcsRequestP(this, HTTP_VERB_GET_STR, .query = query);

                const KeyValue *const content = varKv(jsonToVar(strNewBuf(httpResponseContent(response))));

                // If next page token exists then send an async request to get more data
                const String *const nextPageToken = varStr(kvGet(content, GCS_JSON_NEXT_PAGE_TOKEN_VAR));

                if (nextPageToken != NULL)
                {
                    httpQueryPut(query, GCS_QUERY_PAGE_TOKEN_STR, nextPageToken);

                    // Store request in the outer temp context
                    MEM_CONTEXT_PRIOR_BEGIN()
                    {
                        request = storageGcsRequestAsyncP(this, HTTP_VERB_GET_STR, .query = query);
                    }
                    MEM_CONTEXT_PRIOR_END();
                }

                // Get prefix list
                const VariantList *const prefixList = varVarLst(kvGet(content, GCS_JSON_PREFIXES_VAR));

                if (prefixList != NULL)
                {
                    for (unsigned int prefixIdx = 0; prefixIdx < varLstSize(prefixList); prefixIdx++)
                    {
                        // Get path name
                        StorageInfo info =
                        {
                            .level = level,
                            .name = varStr(varLstGet(prefixList, prefixIdx)),
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
                }

                // Get file list
                const VariantList *const fileList = varVarLst(kvGet(content, GCS_JSON_ITEMS_VAR));

                if (fileList != NULL)
                {
                    for (unsigned int fileIdx = 0; fileIdx < varLstSize(fileList); fileIdx++)
                    {
                        const KeyValue *const file = varKv(varLstGet(fileList, fileIdx));
                        CHECK(FormatError, file != NULL, "file missing");

                        // Get file name and strip off the base prefix when present
                        const String *name = varStr(kvGet(file, GCS_JSON_NAME_VAR));
                        CHECK(FormatError, name != NULL, "file name missing");

                        if (!strEmpty(basePrefix))
                            name = strSub(name, strSize(basePrefix));

                        // Return info for last file if new file
                        if (infoLast.exists && !strEq(name, nameLast))
                        {
                            callback(callbackData, &infoLast);
                            infoLast.exists = false;
                        }

                        // If targeting by time exclude versions that are newer than targetTime. Note that the API does not provide
                        // delete markers so the filtering will also show files that have been deleted rather than replaced with a
                        // new version.
                        if (targetTime != 0)
                        {
                            infoLast.timeModified = storageGcsCvtTime(varStr(kvGet(file, GCS_JSON_UPDATED_VAR)));

                            // Skip this version if it is newer than the time limit
                            if (infoLast.timeModified > targetTime)
                                continue;
                        }

                        // Update last name and set exists
                        strCat(strTrunc(nameLast), name);
                        infoLast.exists = true;

                        // Add basic level info if requested
                        if (level >= storageInfoLevelBasic)
                        {
                            storageGcsInfoFile(&infoLast, file);

                            if (targetTime != 0)
                                strCat(strTrunc(versionIdLast), varStr(kvGet(file, GCS_JSON_GENERATION_VAR)));
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
storageGcsInfo(THIS_VOID, const String *const file, const StorageInfoLevel level, const StorageInterfaceInfoParam param)
{
    THIS(StorageGcs);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_GCS, this);
        FUNCTION_LOG_PARAM(STRING, file);
        FUNCTION_LOG_PARAM(ENUM, level);
        (void)param;                                                // No parameters are used
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(file != NULL);

    StorageInfo result = {.level = level};

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Attempt to get file info
        HttpResponse *const httpResponse = storageGcsRequestP(
            this, HTTP_VERB_GET_STR, .object = file, .allowMissing = true,
            .query = httpQueryAdd(
                httpQueryNewP(), GCS_QUERY_FIELDS_STR,
                level >= storageInfoLevelBasic ? STRDEF(GCS_JSON_SIZE "," GCS_JSON_UPDATED) : EMPTY_STR));

        // Does the file exist?
        result.exists = httpResponseCodeOk(httpResponse);

        // Add basic level info if requested and the file exists
        if (result.level >= storageInfoLevelBasic && result.exists)
        {
            result.type = storageTypeFile;
            storageGcsInfoFile(&result, varKv(jsonToVar(strNewBuf(httpResponseContent(httpResponse)))));
        }

        httpResponseFree(httpResponse);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(STORAGE_INFO, result);
}

/**********************************************************************************************************************************/
static void
storageGcsListCallback(void *const callbackData, const StorageInfo *const info)
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
storageGcsList(THIS_VOID, const String *const path, const StorageInfoLevel level, const StorageInterfaceListParam param)
{
    THIS(StorageGcs);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_GCS, this);
        FUNCTION_LOG_PARAM(STRING, path);
        FUNCTION_LOG_PARAM(ENUM, level);
        FUNCTION_LOG_PARAM(STRING, param.expression);
        FUNCTION_LOG_PARAM(TIME, param.targetTime);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(path != NULL);

    StorageList *const result = storageLstNew(level);

    storageGcsListInternal(this, path, level, param.expression, false, param.targetTime, storageGcsListCallback, result);

    FUNCTION_LOG_RETURN(STORAGE_LIST, result);
}

/**********************************************************************************************************************************/
static StorageRead *
storageGcsNewRead(THIS_VOID, const String *const file, const bool ignoreMissing, const StorageInterfaceNewReadParam param)
{
    THIS(StorageGcs);

    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STORAGE_GCS, this);
        FUNCTION_LOG_PARAM(STRING, file);
        FUNCTION_LOG_PARAM(BOOL, ignoreMissing);
        FUNCTION_LOG_PARAM(UINT64, param.offset);
        FUNCTION_LOG_PARAM(VARIANT, param.limit);
        FUNCTION_LOG_PARAM(BOOL, param.version);
        FUNCTION_LOG_PARAM(STRING, param.versionId);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(file != NULL);

    FUNCTION_LOG_RETURN(
        STORAGE_READ, storageReadGcsNew(this, file, ignoreMissing, param.offset, param.limit, param.version, param.versionId));
}

/**********************************************************************************************************************************/
static StorageWrite *
storageGcsNewWrite(THIS_VOID, const String *const file, const StorageInterfaceNewWriteParam param)
{
    THIS(StorageGcs);

    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STORAGE_GCS, this);
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

    FUNCTION_LOG_RETURN(STORAGE_WRITE, storageWriteGcsNew(this, file, this->chunkSize, this->tag != NULL));
}

/**********************************************************************************************************************************/
#define GCS_HEADER_CONTENTID_RESPONSE                               "response-"

typedef struct StorageGcsPathRemoveData
{
    StorageGcs *this;                                               // Storage Object
    MemContext *memContext;                                         // Mem context to create requests in
    HttpRequest *request;                                           // Async remove request
    List *requestContentList;                                       // Content list for async request
    List *contentList;                                              // Content list currently being built
    const String *path;                                             // Root path of remove
} StorageGcsPathRemoveData;

static void
storageGcsPathRemoveInternal(StorageGcsPathRemoveData *const data)
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
            HttpResponse *const response = storageGcsResponseP(data->request);
            HttpResponseMulti *const responseMulti = httpResponseMultiNew(
                httpResponseContent(response), httpHeaderGet(httpResponseHeader(response), HTTP_HEADER_CONTENT_TYPE_STR));

            // Loop through all response parts
            HttpResponse *responsePart = httpResponseMultiNext(responseMulti);
            CHECK(FormatError, responsePart != NULL, "at least one response part is required");

            do
            {
                // If not OK and not missing then retry
                if (!httpResponseCodeOk(responsePart) && httpResponseCode(responsePart) != HTTP_RESPONSE_CODE_NOT_FOUND)
                {
                    // Extract and check content-id header
                    const String *const contentId = httpHeaderGet(httpResponseHeader(responsePart), HTTP_HEADER_CONTENT_ID_STR);
                    CHECK(FormatError, contentId != NULL, HTTP_HEADER_CONTENT_ID " header is not present");
                    CHECK_FMT(
                        FormatError,
                        strBeginsWithZ(contentId, GCS_HEADER_CONTENTID_RESPONSE),
                        HTTP_HEADER_CONTENT_ID " header '%s' must begin with '" GCS_HEADER_CONTENTID_RESPONSE "'", strZ(contentId));

                    // Use content-id to get content
                    const unsigned int contentIdx = cvtZToUInt(strZ(strSub(contentId, sizeof(GCS_HEADER_CONTENTID_RESPONSE) - 1)));
                    const StorageGcsRequestPart *const content = lstGet(data->requestContentList, contentIdx);

                    // Retry remove
                    statInc(GCS_STAT_REMOVE_BATCH_RETRY_STR);
                    storageGcsRequestP(data->this, content->verb, .object = content->object, .allowMissing = true);
                }
                else
                    statInc(GCS_STAT_REMOVE_BATCH_PART_STR);

                httpResponseFree(responsePart);
                responsePart = httpResponseMultiNext(responseMulti);
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
        statInc(GCS_STAT_REMOVE_BATCH_STR);

        MEM_CONTEXT_BEGIN(data->memContext)
        {
            data->request = storageGcsRequestAsyncP(
                data->this, HTTP_VERB_POST_STR, .path = STRDEF("/batch/storage/v1"), .contentList = data->contentList);
        }
        MEM_CONTEXT_END();

        // Store the content list for use in error handling
        data->requestContentList = data->contentList;
        data->contentList = NULL;
    }

    FUNCTION_TEST_RETURN_VOID();
}

static void
storageGcsPathRemoveCallback(void *const callbackData, const StorageInfo *const info)
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
        StorageGcsPathRemoveData *const data = callbackData;

        if (data->contentList == NULL)
        {
            MEM_CONTEXT_BEGIN(data->memContext)
            {
                data->contentList = lstNewP(sizeof(StorageGcsRequestPart));
            }
            MEM_CONTEXT_END();
        }

        MEM_CONTEXT_OBJ_BEGIN(data->contentList)
        {
            const StorageGcsRequestPart content =
            {
                .verb = HTTP_VERB_DELETE_STR,
                .object = strNewFmt("%s/%s", strZ(data->path), strZ(info->name)),
            };

            lstAdd(data->contentList, &content);
        }
        MEM_CONTEXT_OBJ_END();

        if (lstSize(data->contentList) == data->this->deleteMax)
            storageGcsPathRemoveInternal(data);
    }

    FUNCTION_TEST_RETURN_VOID();
}

static bool
storageGcsPathRemove(THIS_VOID, const String *const path, const bool recurse, const StorageInterfacePathRemoveParam param)
{
    THIS(StorageGcs);

    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STORAGE_GCS, this);
        FUNCTION_LOG_PARAM(STRING, path);
        FUNCTION_LOG_PARAM(BOOL, recurse);
        (void)param;                                                // No parameters are used
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(path != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        StorageGcsPathRemoveData data =
        {
            .this = this,
            .memContext = memContextCurrent(),
            .path = strEq(path, FSLASH_STR) ? EMPTY_STR : path,
        };

        MEM_CONTEXT_TEMP_BEGIN()
        {
            storageGcsListInternal(this, path, storageInfoLevelType, NULL, true, 0, storageGcsPathRemoveCallback, &data);

            // Call if there is more to be removed
            if (data.contentList != NULL)
                storageGcsPathRemoveInternal(&data);

            // Check response on last async request
            storageGcsPathRemoveInternal(&data);
        }
        MEM_CONTEXT_TEMP_END();
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(BOOL, true);
}

/**********************************************************************************************************************************/
static void
storageGcsRemove(THIS_VOID, const String *const file, const StorageInterfaceRemoveParam param)
{
    THIS(StorageGcs);

    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STORAGE_GCS, this);
        FUNCTION_LOG_PARAM(STRING, file);
        FUNCTION_LOG_PARAM(BOOL, param.errorOnMissing);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(file != NULL);
    ASSERT(!param.errorOnMissing);

    statInc(GCS_STAT_REMOVE_STR);
    httpResponseFree(storageGcsRequestP(this, HTTP_VERB_DELETE_STR, .object = file, .allowMissing = true));

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
static const StorageInterface storageInterfaceGcs =
{
    .feature = 1 << storageFeatureVersioning,

    .info = storageGcsInfo,
    .list = storageGcsList,
    .newRead = storageGcsNewRead,
    .newWrite = storageGcsNewWrite,
    .pathRemove = storageGcsPathRemove,
    .remove = storageGcsRemove,
};

FN_EXTERN Storage *
storageGcsNew(
    const String *const path, const bool write, const time_t targetTime, StoragePathExpressionCallback pathExpressionFunction,
    const String *const bucket, const StorageGcsKeyType keyType, const String *const key, const size_t chunkSize,
    const KeyValue *const tag, const String *const endpoint, const TimeMSec timeout, const bool verifyPeer,
    const String *const caFile, const String *const caPath, const String *const userProject)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STRING, path);
        FUNCTION_LOG_PARAM(BOOL, write);
        FUNCTION_LOG_PARAM(TIME, targetTime);
        FUNCTION_LOG_PARAM(FUNCTIONP, pathExpressionFunction);
        FUNCTION_LOG_PARAM(STRING, bucket);
        FUNCTION_LOG_PARAM(STRING_ID, keyType);
        FUNCTION_TEST_PARAM(STRING, key);
        FUNCTION_LOG_PARAM(SIZE, chunkSize);
        FUNCTION_LOG_PARAM(KEY_VALUE, tag);
        FUNCTION_LOG_PARAM(STRING, endpoint);
        FUNCTION_LOG_PARAM(TIME_MSEC, timeout);
        FUNCTION_LOG_PARAM(BOOL, verifyPeer);
        FUNCTION_LOG_PARAM(STRING, caFile);
        FUNCTION_LOG_PARAM(STRING, caPath);
        FUNCTION_LOG_PARAM(STRING, userProject);
    FUNCTION_LOG_END();

    ASSERT(path != NULL);
    ASSERT(bucket != NULL);
    ASSERT(keyType == storageGcsKeyTypeAuto || key != NULL);
    ASSERT(chunkSize != 0);

    OBJ_NEW_BEGIN(StorageGcs, .childQty = MEM_CONTEXT_QTY_MAX)
    {
        *this = (StorageGcs)
        {
            .interface = storageInterfaceGcs,
            .write = write,
            .bucket = strDup(bucket),
            .keyType = keyType,
            .chunkSize = chunkSize,
            .deleteMax = STORAGE_GCS_DELETE_MAX,
            .userProject = strDup(userProject),
        };

        // Create tag JSON buffer
        if (write && tag != NULL)
        {
            MEM_CONTEXT_TEMP_BEGIN()
            {
                JsonWrite *const tagJson = jsonWriteObjectBegin(
                    jsonWriteKeyStrId(jsonWriteObjectBegin(jsonWriteNewP()), STRID5("metadata", 0xd0240d0ad0)));
                const StringList *const keyList = strLstSort(strLstNewVarLst(kvKeyList(tag)), sortOrderAsc);

                for (unsigned int keyIdx = 0; keyIdx < strLstSize(keyList); keyIdx++)
                {
                    const String *const key = strLstGet(keyList, keyIdx);
                    jsonWriteStr(jsonWriteKey(tagJson, key), varStr(kvGet(tag, VARSTR(key))));
                }

                const String *const tagStr = jsonWriteResult(jsonWriteObjectEnd(jsonWriteObjectEnd(tagJson)));

                MEM_CONTEXT_PRIOR_BEGIN()
                {
                    this->tag = bufDup(BUFSTR(tagStr));
                }
                MEM_CONTEXT_PRIOR_END();
            }
            MEM_CONTEXT_TEMP_END();
        }

        // Handle auth key types
        switch (keyType)
        {
            // Auto authentication for GCE instances
            case storageGcsKeyTypeAuto:
            {
                this->authUrl = httpUrlNewParseP(
                    STRDEF("metadata.google.internal/computeMetadata/v1/instance/service-accounts/default/token"),
                    .type = httpProtocolTypeHttp);
                this->authClient = httpClientNew(
                    sckClientNew(httpUrlHost(this->authUrl), httpUrlPort(this->authUrl), timeout, timeout), timeout);

                break;
            }

            // Read data from file for service keys
            case storageGcsKeyTypeService:
            {
                const KeyValue *const kvKey = varKv(
                    jsonToVar(strNewBuf(storageGetP(storageNewReadP(storagePosixNewP(FSLASH_STR), key)))));
                const String *const uri = varStr(kvGet(kvKey, GCS_JSON_TOKEN_URI_VAR));
                CHECK(FormatError, uri != NULL, "uri missing");

                this->key = strDup(key);
                this->authUrl = httpUrlNewParseP(uri, .type = httpProtocolTypeHttps);
                this->authClient = httpClientNew(
                    tlsClientNewP(
                        sckClientNew(httpUrlHost(this->authUrl), httpUrlPort(this->authUrl), timeout, timeout),
                        httpUrlHost(this->authUrl), timeout, timeout, verifyPeer, .caFile = caFile, .caPath = caPath),
                    timeout);

                break;
            }

            // Store the authentication token
            case storageGcsKeyTypeToken:
                this->token = strDup(key);
                break;
        }

        // Parse the endpoint to extract the host and port
        const HttpUrl *const url = httpUrlNewParseP(endpoint, .type = httpProtocolTypeHttps);
        this->endpoint = httpUrlHost(url);

        // Create the http client used to service requests
        this->httpClient = httpClientNew(
            tlsClientNewP(
                sckClientNew(this->endpoint, httpUrlPort(url), timeout, timeout), this->endpoint, timeout, timeout, verifyPeer,
                .caFile = caFile, .caPath = caPath),
            timeout);

        // Create list of redacted headers
        this->headerRedactList = strLstNew();
        strLstAdd(this->headerRedactList, HTTP_HEADER_AUTHORIZATION_STR);
        strLstAdd(this->headerRedactList, GCS_HEADER_UPLOAD_ID_STR);

        // Create list of redacted query keys
        this->queryRedactList = strLstNew();
        strLstAdd(this->queryRedactList, GCS_QUERY_UPLOAD_ID_STR);
    }
    OBJ_NEW_END();

    FUNCTION_LOG_RETURN(
        STORAGE, storageNew(STORAGE_GCS_TYPE, path, 0, 0, write, targetTime, pathExpressionFunction, this, this->interface));
}
