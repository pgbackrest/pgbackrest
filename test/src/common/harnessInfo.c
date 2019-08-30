/***********************************************************************************************************************************
Harness for Loading Test Configurations
***********************************************************************************************************************************/
#include "common/harnessDebug.h"
#include "common/harnessInfo.h"

#include "common/assert.h"
#include "common/crypto/hash.h"
#include "common/io/bufferRead.h"
#include "common/io/filter/filter.intern.h"
#include "common/type/json.h"
#include "info/info.h"
#include "version.h"

/***********************************************************************************************************************************
Load a test configuration without any side effects

There's no need to open log files, acquire locks, reset log levels, etc.
***********************************************************************************************************************************/
typedef struct HarnessInfoChecksumData
{
    MemContext *memContext;                                         // Mem context to use for storing data in this structure
    String *sectionLast;                                            // The last section seen during load
    IoFilter *checksum;                                             // Checksum calculated from the file
} HarnessInfoChecksumData;

static void
harnessInfoChecksumCallback(void *callbackData, const String *section, const String *key, const String *value)
{
    HarnessInfoChecksumData *data = (HarnessInfoChecksumData *)callbackData;

    // Calculate checksum
    if (data->sectionLast == NULL || !strEq(section, data->sectionLast))
    {
        if (data->sectionLast != NULL)
            ioFilterProcessIn(data->checksum, BUFSTRDEF("},"));

        ioFilterProcessIn(data->checksum, BUFSTRDEF("\""));
        ioFilterProcessIn(data->checksum, BUFSTR(section));
        ioFilterProcessIn(data->checksum, BUFSTRDEF("\":{"));

        MEM_CONTEXT_BEGIN(data->memContext)
        {
            data->sectionLast = strDup(section);
        }
        MEM_CONTEXT_END();
    }
    else
        ioFilterProcessIn(data->checksum, BUFSTRDEF(","));

    ioFilterProcessIn(data->checksum, BUFSTRDEF("\""));
    ioFilterProcessIn(data->checksum, BUFSTR(key));
    ioFilterProcessIn(data->checksum, BUFSTRDEF("\":"));
    ioFilterProcessIn(data->checksum, BUFSTR(value));
}

Buffer *
harnessInfoChecksum(const String *info)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(STRING, info);
    FUNCTION_HARNESS_END();

    Buffer *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Initialize callback data
        HarnessInfoChecksumData data =
        {
            .memContext = MEM_CONTEXT_TEMP(),
            .checksum = cryptoHashNew(HASH_TYPE_SHA1_STR),
        };

        // Create buffer with header and data
        result = bufNew(strSize(info) + 256);

        bufCat(result, BUFSTRDEF("[backrest]\nbackrest-format="));
        bufCat(result, BUFSTR(jsonFromUInt(REPOSITORY_FORMAT)));
        bufCat(result, BUFSTRDEF("\nbackrest-version="));
        bufCat(result, BUFSTR(jsonFromStr(STRDEF(PROJECT_VERSION))));
        bufCat(result, BUFSTRDEF("\n\n"));
        bufCat(result, BUFSTR(info));

        // Generate checksum by loading ini file
        ioFilterProcessIn(data.checksum, BUFSTRDEF("{"));
        iniLoad(ioBufferReadNew(result), harnessInfoChecksumCallback, &data);
        ioFilterProcessIn(data.checksum, BUFSTRDEF("}}"));

        // Append checksum to buffer
        bufCat(result, BUFSTRDEF("\n[backrest]\nbackrest-checksum="));
        bufCat(result, BUFSTR(jsonFromVar(ioFilterResult(data.checksum), 0)));
        bufCat(result, BUFSTRDEF("\n"));

        bufMove(result, MEM_CONTEXT_OLD());
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_HARNESS_RESULT(BUFFER, result);
}

Buffer *
harnessInfoChecksumZ(const char *info)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(STRINGZ, info);
    FUNCTION_HARNESS_END();

    FUNCTION_HARNESS_RESULT(BUFFER, harnessInfoChecksum(STRDEF(info)));
}

/***********************************************************************************************************************************
Test callback that logs the results to a string
***********************************************************************************************************************************/
void
harnessInfoLoadCallback(InfoCallbackType type, void *callbackData, const String *section, const String *key, const String *value)
{
    if (callbackData != NULL)
    {
        switch (type)
        {
            case infoCallbackTypeBegin:
            {
                strCat((String *)callbackData, "BEGIN\n");
                break;
            }

            case infoCallbackTypeReset:
            {
                strCat((String *)callbackData, "RESET\n");
                break;
            }

            case infoCallbackTypeValue:
            {
                strCatFmt((String *)callbackData, "[%s] %s=%s\n", strPtr(section), strPtr(key), strPtr(value));
                break;
            }

            case infoCallbackTypeEnd:
            {
                strCat((String *)callbackData, "END\n");
                break;
            }
        }
    }
}
