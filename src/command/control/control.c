/***********************************************************************************************************************************
Command Control
***********************************************************************************************************************************/
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
void
lockStopTest(void)
{
    FUNCTION_LOG_VOID(logLevelDebug);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Check the current stanza (if any)
        if (cfgOptionTest(cfgOptStanza))
        {
            if (storageExistsNP(storageLocal(), lockStopFileName(cfgOptionStr(cfgOptStanza))))
                THROW_FMT(StopError, "stop file exists for stanza %s", strPtr(cfgOptionStr(cfgOptStanza)));
        }

        // Check all stanzas
        if (storageExistsNP(storageLocal(), lockStopFileName(NULL)))
            THROW(StopError, "stop file exists for all stanzas");
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}
