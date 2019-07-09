/***********************************************************************************************************************************
Command Control
***********************************************************************************************************************************/
#include "build.auto.h"

#include "command/control/control.h"
#include "common/debug.h"
#include "config/config.h"
#include "storage/helper.h"

/***********************************************************************************************************************************
Create the stop filename
***********************************************************************************************************************************/
String *
lockStopFileName(const String *stanza)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, stanza);
    FUNCTION_TEST_END();

    String *result = strNewFmt("%s/%s.stop", strPtr(cfgOptionStr(cfgOptLockPath)), stanza != NULL ? strPtr(stanza) : "all");

    FUNCTION_TEST_RETURN(result);
}

/***********************************************************************************************************************************
Test for the existence of a stop file
***********************************************************************************************************************************/
bool
lockStopTest(bool stanzaStopRequired)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(BOOL, stanzaStopRequired);
    FUNCTION_LOG_END();

    bool result = false;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Check the current stanza (if any)
        if (cfgOptionTest(cfgOptStanza))
        {
            result = storageExistsNP(storageLocal(), lockStopFileName(cfgOptionStr(cfgOptStanza)));

            // If the stop file exists and is not required then error
            if (result && !stanzaStopRequired)
                THROW_FMT(StopError, "stop file exists for stanza %s", strPtr(cfgOptionStr(cfgOptStanza)));
        }

        // If not looking for a specific stanza stop file, then check all stanzas
        if (!stanzaStopRequired && storageExistsNP(storageLocal(), lockStopFileName(NULL)))
            THROW(StopError, "stop file exists for all stanzas");
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(BOOL, result);
}
