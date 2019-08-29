/***********************************************************************************************************************************
Harness for Loading Test Configurations
***********************************************************************************************************************************/
#include "common/harnessDebug.h"
#include "common/harnessInfo.h"

#include "common/crypto/hash.h"
#include "common/io/bufferWrite.h"
#include "common/io/filter/filter.intern.h"
#include "common/type/json.h"
#include "info/info.h"
#include "version.h"

/***********************************************************************************************************************************
Generate hash for the contents of an ini file
***********************************************************************************************************************************/
static String *
infoHash(const Ini *ini)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(INI, ini);
    FUNCTION_TEST_END();

    ASSERT(ini != NULL);

    String *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        IoFilter *hash = cryptoHashNew(HASH_TYPE_SHA1_STR);
        StringList *sectionList = strLstSort(iniSectionList(ini), sortOrderAsc);

        // Initial JSON opening bracket
        ioFilterProcessIn(hash, BUFSTRDEF("{"));

        // Loop through sections and create hash for checking checksum
        for (unsigned int sectionIdx = 0; sectionIdx < strLstSize(sectionList); sectionIdx++)
        {
            String *section = strLstGet(sectionList, sectionIdx);

            // Add a comma before additional sections
            if (sectionIdx != 0)
                ioFilterProcessIn(hash, BUFSTRDEF(","));

            // Create the section header
            ioFilterProcessIn(hash, BUFSTRDEF("\""));
            ioFilterProcessIn(hash, BUFSTR(section));
            ioFilterProcessIn(hash, BUFSTRDEF("\":{"));

            StringList *keyList = strLstSort(iniSectionKeyList(ini, section), sortOrderAsc);
            unsigned int keyListSize = strLstSize(keyList);

            // Loop through values and build the section
            for (unsigned int keyIdx = 0; keyIdx < keyListSize; keyIdx++)
            {
                String *key = strLstGet(keyList, keyIdx);

                // Skip the backrest checksum in the file
                ioFilterProcessIn(hash, BUFSTRDEF("\""));
                ioFilterProcessIn(hash, BUFSTR(key));
                ioFilterProcessIn(hash, BUFSTRDEF("\":"));
                ioFilterProcessIn(hash, BUFSTR(iniGet(ini, section, strLstGet(keyList, keyIdx))));

                if ((keyListSize > 1) && (keyIdx < keyListSize - 1))
                    ioFilterProcessIn(hash, BUFSTRDEF(","));
            }

            // Close the key/value list
            ioFilterProcessIn(hash, BUFSTRDEF("}"));
        }

        // JSON closing bracket
        ioFilterProcessIn(hash, BUFSTRDEF("}"));

        Variant *resultVar = ioFilterResult(hash);

        memContextSwitch(MEM_CONTEXT_OLD());
        result = strDup(varStr(resultVar));
        memContextSwitch(MEM_CONTEXT_TEMP());
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_TEST_RETURN(result);
}

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

        // Write to a buffer
        result = bufNew(0);
        iniSave(ini, ioBufferWriteNew(result));
        bufCat(result, BUFSTRDEF("\n[backrest]\nbackrest-checksum="));
        bufCat(result, BUFSTR(jsonFromStr(infoHash(ini))));
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
