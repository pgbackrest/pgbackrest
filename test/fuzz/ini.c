/***********************************************************************************************************************************
Fuzz Ini
***********************************************************************************************************************************/
#include <build.h>

#include "common/debug.h"
#include "common/ini.h"
#include "common/io/bufferRead.h"

#include "fuzz.h"

/**********************************************************************************************************************************/
void
fuzzTarget(const Buffer *const input)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(BUFFER, input);
    FUNCTION_TEST_END();

    // Read strict ini with JSON values, e.g. info files and manifests
    TRY_BEGIN()
    {
        iniValid(iniNewP(ioBufferReadNew(input), .strict = true));
    }
    CATCH_ANY()
    {
        // Errors thrown for invalid input are expected
    }
    TRY_END();

    // Read ini into a store and get every value, e.g. config files
    const Ini *const ini = iniNewP(ioBufferReadNew(input), .store = true);
    const StringList *const sectionList = iniSectionList(ini);

    for (unsigned int sectionIdx = 0; sectionIdx < strLstSize(sectionList); sectionIdx++)
    {
        const String *const section = strLstGet(sectionList, sectionIdx);
        const StringList *const keyList = iniSectionKeyList(ini, section);

        for (unsigned int keyIdx = 0; keyIdx < strLstSize(keyList); keyIdx++)
        {
            const String *const key = strLstGet(keyList, keyIdx);

            if (iniSectionKeyIsList(ini, section, key))
                iniGetList(ini, section, key);
            else
                iniGet(ini, section, key);
        }
    }

    FUNCTION_TEST_RETURN_VOID();
}
