/***********************************************************************************************************************************
Harness for Loading Test Configurations
***********************************************************************************************************************************/
#include "build.auto.h"

#include <string.h>

#include "common/assert.h"
#include "common/crypto/hash.h"
#include "common/io/bufferRead.h"
#include "common/io/filter/filter.h"
#include "common/type/json.h"
#include "info/info.h"
#include "version.h"

#include "common/harnessDebug.h"
#include "common/harnessInfo.h"

/***********************************************************************************************************************************
Add header and checksum to an info file

This prevents churn in headers and checksums in the unit tests.  We purposefully do not use the checksum macros from the info module
here as a cross-check of that code.
***********************************************************************************************************************************/
typedef struct HarnessInfoChecksumData
{
    MemContext *memContext;                                         // Mem context to use for storing data in this structure
    String *sectionLast;                                            // The last section seen during load
    IoFilter *checksum;                                             // Checksum calculated from the file
} HarnessInfoChecksumData;

static void
harnessInfoChecksumCallback(
    void *const callbackData, const String *const section, const String *const key, const String *const value)
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

    ioFilterProcessIn(data->checksum, BUFSTR(jsonFromVar(VARSTR(key))));
    ioFilterProcessIn(data->checksum, BUFSTRDEF(":"));
    ioFilterProcessIn(data->checksum, BUFSTR(value));
}

Buffer *
harnessInfoChecksum(const String *info)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(STRING, info);
    FUNCTION_HARNESS_END();

    ASSERT(info != NULL);

    Buffer *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Initialize callback data
        HarnessInfoChecksumData data =
        {
            .memContext = MEM_CONTEXT_TEMP(),
            .checksum = cryptoHashNew(hashTypeSha1),
        };

        // Create buffer with space for data, header, and checksum
        result = bufNew(strSize(info) + 256);

        bufCat(result, BUFSTRDEF("[backrest]\nbackrest-format="));
        bufCat(result, BUFSTR(jsonFromVar(VARUINT(REPOSITORY_FORMAT))));
        bufCat(result, BUFSTRDEF("\nbackrest-version="));
        bufCat(result, BUFSTR(jsonFromVar(VARSTRDEF(PROJECT_VERSION))));
        bufCat(result, BUFSTRDEF("\n\n"));
        bufCat(result, BUFSTR(info));

        // Generate checksum by loading ini file
        ioFilterProcessIn(data.checksum, BUFSTRDEF("{"));
        iniLoad(ioBufferReadNew(result), harnessInfoChecksumCallback, &data);
        ioFilterProcessIn(data.checksum, BUFSTRDEF("}}"));

        // Append checksum to buffer
        bufCat(result, BUFSTRDEF("\n[backrest]\nbackrest-checksum="));
        bufCat(result, BUFSTR(jsonFromVar(VARSTR(bufHex(pckReadBinP(pckReadNew(ioFilterResult(data.checksum))))))));
        bufCat(result, BUFSTRDEF("\n"));

        bufMove(result, memContextPrior());
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_HARNESS_RETURN(BUFFER, result);
}

Buffer *
harnessInfoChecksumZ(const char *info)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(STRINGZ, info);
    FUNCTION_HARNESS_END();

    ASSERT(info != NULL);

    FUNCTION_HARNESS_RETURN(BUFFER, harnessInfoChecksum(STR(info)));
}

/***********************************************************************************************************************************
Test callback that logs the results to a string
***********************************************************************************************************************************/
void
harnessInfoLoadNewCallback(
    void *const callbackData, const String *const section, const String *const key, const String *const value)
{
    if (callbackData != NULL)
        strCatFmt((String *)callbackData, "[%s] %s=%s\n", strZ(section), strZ(key), strZ(value));
}
