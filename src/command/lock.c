/***********************************************************************************************************************************
Command Lock Handler
***********************************************************************************************************************************/
#include "build.auto.h"

#include "command/lock.h"
#include "common/debug.h"
#include "common/log.h"
#include "config/config.h"

/**********************************************************************************************************************************/
FN_EXTERN LockReadResult
cmdLockRead(const LockType lockType, const String *const stanza)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(ENUM, lockType);
        FUNCTION_LOG_PARAM(STRING, stanza);
    FUNCTION_LOG_END();

    FUNCTION_AUDIT_STRUCT();

    LockReadResult result = {0};

    MEM_CONTEXT_TEMP_BEGIN()
    {
        const String *const lockFile = strNewFmt("%s/%s", strZ(cfgOptionStr(cfgOptLockPath)), strZ(lockFileName(stanza, lockType)));

        MEM_CONTEXT_PRIOR_BEGIN()
        {
            result = lockReadFileP(lockFile);
        }
        MEM_CONTEXT_PRIOR_END();
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_STRUCT(result);
}
