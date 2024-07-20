/***********************************************************************************************************************************
S3 Storage Helper
***********************************************************************************************************************************/
#include "build.auto.h"

#include <stdlib.h>

#include "common/debug.h"
#include "common/io/http/url.h"
#include "common/io/io.h"
#include "common/log.h"
#include "common/regExp.h" // !!!
#include "config/config.h"
#include "storage/posix/storage.h"
#include "storage/s3/helper.h"

/***********************************************************************************************************************************
Get epoch time from formatted string
***********************************************************************************************************************************/
// !!! COPIED FROM RESTORE. NEEDS TO BE SIMPLIFIED AND MOVED TO TIME.C
static time_t
epochFromStr(const String *const targetTime)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STRING, targetTime);
    FUNCTION_LOG_END();

    ASSERT(targetTime != NULL);

    time_t result = 0;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Build the regex to accept formats: YYYY-MM-DD HH:MM:SS with optional msec (up to 6 digits and separated from minutes by
        // a comma or period), optional timezone offset +/- HH or HHMM or HH:MM, where offset boundaries are UTC-12 to UTC+14
        const String *const expression = STRDEF(
            "^[0-9]{4}-[0-9]{2}-[0-9]{2} [0-9]{2}:[0-9]{2}:[0-9]{2}((\\,|\\.)[0-9]{1,6})?((\\+|\\-)[0-9]{2}(:?)([0-9]{2})?)?$");
        RegExp *const regExp = regExpNew(expression);

        // If the target-recovery time matches the regular expression then validate it
        if (regExpMatch(regExp, targetTime))
        {
            // Strip off the date and time and put the remainder into another string
            const String *const datetime = strSubN(targetTime, 0, 19);

            const int dtYear = cvtZSubNToInt(strZ(datetime), 0, 4);
            const int dtMonth = cvtZSubNToInt(strZ(datetime), 5, 2);
            const int dtDay = cvtZSubNToInt(strZ(datetime), 8, 2);
            const int dtHour = cvtZSubNToInt(strZ(datetime), 11, 2);
            const int dtMinute = cvtZSubNToInt(strZ(datetime), 14, 2);
            const int dtSecond = cvtZSubNToInt(strZ(datetime), 17, 2);

            // Confirm date and time parts are valid
            datePartsValid(dtYear, dtMonth, dtDay);
            timePartsValid(dtHour, dtMinute, dtSecond);

            const String *const timeTargetZone = strSub(targetTime, 19);

            // Find the + or - indicating a timezone offset was provided (there may be milliseconds before the timezone, so need to
            // skip). If a timezone offset was not provided, then local time is assumed.
            int idxSign = strChr(timeTargetZone, '+');

            if (idxSign == -1)
                idxSign = strChr(timeTargetZone, '-');

            if (idxSign != -1)
            {
                const String *const timezoneOffset = strSub(timeTargetZone, (size_t)idxSign);

                // Include the sign with the hour
                const int tzHour = cvtZSubNToInt(strZ(timezoneOffset), 0, 3);
                int tzMinute = 0;

                // If minutes are included in timezone offset then extract the minutes based on whether a colon separates them from
                // the hour
                if (strSize(timezoneOffset) > 3)
                    tzMinute = cvtZSubNToInt(strZ(timezoneOffset), 3 + (strChr(timezoneOffset, ':') == -1 ? 0 : 1), 2);

                result = epochFromParts(dtYear, dtMonth, dtDay, dtHour, dtMinute, dtSecond, tzOffsetSeconds(tzHour, tzMinute));
            }
            // If there is no timezone offset, then assume it is local time
            else
            {
                // Set tm_isdst to -1 to force mktime to consider if DST. For example, if system time is America/New_York then
                // 2019-09-14 20:02:49 was a time in DST so the Epoch value should be 1568505769 (and not 1568509369 which would be
                // 2019-09-14 21:02:49 - an hour too late)
                struct tm time =
                {
                    .tm_sec = dtSecond,
                    .tm_min = dtMinute,
                    .tm_hour = dtHour,
                    .tm_mday = dtDay,
                    .tm_mon = dtMonth - 1,
                    .tm_year = dtYear - 1900,
                    .tm_isdst = -1,
                };

                result = mktime(&time);
            }
        }
        else
        {
            THROW_FMT(
                FormatError,
                "automatic backup set selection cannot be performed with provided time '%s'\n"
                "HINT: time format must be YYYY-MM-DD HH:MM:SS with optional msec and optional timezone (+/- HH or HHMM or HH:MM)"
                " - if timezone is omitted, local time is assumed (for UTC use +00)",
                strZ(targetTime));
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(TIME, result);
}

/**********************************************************************************************************************************/
FN_EXTERN Storage *
storageS3Helper(const unsigned int repoIdx, const bool write, StoragePathExpressionCallback pathExpressionCallback)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(UINT, repoIdx);
        FUNCTION_LOG_PARAM(BOOL, write);
        FUNCTION_LOG_PARAM_P(VOID, pathExpressionCallback);
    FUNCTION_LOG_END();

    ASSERT(cfgOptionIdxStrId(cfgOptRepoType, repoIdx) == STORAGE_S3_TYPE);

    Storage *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Parse the endpoint url
        const HttpUrl *const url = httpUrlNewParseP(cfgOptionIdxStr(cfgOptRepoS3Endpoint, repoIdx), .type = httpProtocolTypeHttps);
        const String *const endPoint = httpUrlHost(url);
        unsigned int port = httpUrlPort(url);

        // If host was specified then use it
        const String *host = NULL;

        if (cfgOptionIdxSource(cfgOptRepoStorageHost, repoIdx) != cfgSourceDefault)
        {
            const HttpUrl *const url = httpUrlNewParseP(
                cfgOptionIdxStr(cfgOptRepoStorageHost, repoIdx), .type = httpProtocolTypeHttps);

            host = httpUrlHost(url);
            port = httpUrlPort(url);
        }

        // If port was specified, overwrite the parsed/default port
        if (cfgOptionIdxSource(cfgOptRepoStoragePort, repoIdx) != cfgSourceDefault)
            port = cfgOptionIdxUInt(cfgOptRepoStoragePort, repoIdx);

        // Get role and token
        const StorageS3KeyType keyType = (StorageS3KeyType)cfgOptionIdxStrId(cfgOptRepoS3KeyType, repoIdx);
        const String *role = cfgOptionIdxStrNull(cfgOptRepoS3Role, repoIdx);
        const String *webIdToken = NULL;

        // If web identity authentication then load the role and token from environment variables documented here:
        // https://docs.aws.amazon.com/eks/latest/userguide/iam-roles-for-service-accounts-technical-overview.html
        if (keyType == storageS3KeyTypeWebId)
        {
            #define S3_ENV_AWS_ROLE_ARN                             "AWS_ROLE_ARN"
            #define S3_ENV_AWS_WEB_IDENTITY_TOKEN_FILE              "AWS_WEB_IDENTITY_TOKEN_FILE"

            const char *const roleZ = getenv(S3_ENV_AWS_ROLE_ARN);
            const char *const webIdTokenFileZ = getenv(S3_ENV_AWS_WEB_IDENTITY_TOKEN_FILE);

            if (roleZ == NULL || webIdTokenFileZ == NULL)
            {
                THROW_FMT(
                    OptionInvalidError,
                    "option '%s' is '" CFGOPTVAL_REPO_S3_KEY_TYPE_WEB_ID_Z "' but '" S3_ENV_AWS_ROLE_ARN "' and"
                    " '" S3_ENV_AWS_WEB_IDENTITY_TOKEN_FILE "' are not set",
                    cfgOptionIdxName(cfgOptRepoS3KeyType, repoIdx));
            }

            role = strNewZ(roleZ);
            webIdToken = strNewBuf(storageGetP(storageNewReadP(storagePosixNewP(FSLASH_STR), STR(webIdTokenFileZ))));
        }

        MEM_CONTEXT_PRIOR_BEGIN()
        {
            result = storageS3New(
                cfgOptionIdxStr(cfgOptRepoPath, repoIdx), write,
                cfgOptionTest(cfgOptLimitTime) ? epochFromStr(cfgOptionStr(cfgOptLimitTime)) : 0,
                pathExpressionCallback, cfgOptionIdxStr(cfgOptRepoS3Bucket, repoIdx), endPoint,
                (StorageS3UriStyle)cfgOptionIdxStrId(cfgOptRepoS3UriStyle, repoIdx), cfgOptionIdxStr(cfgOptRepoS3Region, repoIdx),
                keyType, cfgOptionIdxStrNull(cfgOptRepoS3Key, repoIdx), cfgOptionIdxStrNull(cfgOptRepoS3KeySecret, repoIdx),
                cfgOptionIdxStrNull(cfgOptRepoS3Token, repoIdx), cfgOptionIdxStrNull(cfgOptRepoS3KmsKeyId, repoIdx),
                cfgOptionIdxStrNull(cfgOptRepoS3SseCustomerKey, repoIdx), role, webIdToken,
                (size_t)cfgOptionIdxUInt64(cfgOptRepoStorageUploadChunkSize, repoIdx),
                cfgOptionIdxKvNull(cfgOptRepoStorageTag, repoIdx), host, port, ioTimeoutMs(),
                cfgOptionIdxBool(cfgOptRepoStorageVerifyTls, repoIdx), cfgOptionIdxStrNull(cfgOptRepoStorageCaFile, repoIdx),
                cfgOptionIdxStrNull(cfgOptRepoStorageCaPath, repoIdx));
        }
        MEM_CONTEXT_PRIOR_END();
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(STORAGE, result);
}
