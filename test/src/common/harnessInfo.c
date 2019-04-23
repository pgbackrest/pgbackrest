/***********************************************************************************************************************************
Harness for Loading Test Configurations
***********************************************************************************************************************************/
#include "common/harnessDebug.h"
#include "common/harnessInfo.h"

#include "common/io/bufferWrite.h"
#include "common/type/json.h"
#include "info/info.h"
#include "version.h"

/***********************************************************************************************************************************
Load a test configuration without any side effects

There's no need to open log files, acquire locks, reset log levels, etc.
***********************************************************************************************************************************/
Buffer *
harnessInfoChecksum(const String *info)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(STRING, info);
    FUNCTION_HARNESS_END();

    Buffer *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Load data into an ini file
        Ini *ini = iniNew();
        iniParse(ini, info);

        // Add header and checksum values
        iniSet(ini, STRDEF("backrest"), STRDEF("backrest-version"), jsonFromStr(STRDEF(PROJECT_VERSION)));
        iniSet(ini, STRDEF("backrest"), STRDEF("backrest-format"), jsonFromUInt(REPOSITORY_FORMAT));
        iniSet(ini, STRDEF("backrest"), STRDEF("backrest-checksum"), jsonFromStr(infoHash(ini)));

        // Write to a buffer
        result = bufNew(0);
        iniSave(ini, ioBufferWriteIo(ioBufferWriteNew(result)));
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
