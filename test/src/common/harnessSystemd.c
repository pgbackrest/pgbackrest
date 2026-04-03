/***********************************************************************************************************************************
Systemd Harness
***********************************************************************************************************************************/
#include <build.h>

#include "common/debug.h"
#include "common/log.h"

#include "common/harnessDebug.h"
#include "common/harnessSystemd.h"

static struct
{
    bool ready;
    bool stopping;
} hrnSystemDStatic;

/**********************************************************************************************************************************/
int
sd_notify(int unset_environment, const char *state)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(INT, unset_environment);
        FUNCTION_HARNESS_PARAM(STRINGZ, state);
    FUNCTION_HARNESS_END();

    ASSERT(unset_environment == 0);
    ASSERT(state != NULL);

    if (strcmp(state, "READY=1") == 0)
    {
        ASSERT(!hrnSystemDStatic.stopping);
        hrnSystemDStatic.ready = true;
    }
    else if (strcmp(state, "STOPPING=1") == 0)
    {
        ASSERT(hrnSystemDStatic.ready);
        hrnSystemDStatic.stopping = true;
    }
    else
        THROW_FMT(AssertError, "unknown sd_notify state '%s'", state);

    FUNCTION_HARNESS_RETURN(INT, 0);
}

/**********************************************************************************************************************************/
void
hrnSystemDCheck(void)
{
    FUNCTION_HARNESS_VOID();

    ASSERT(hrnSystemDStatic.ready && hrnSystemDStatic.stopping);

    FUNCTION_HARNESS_RETURN_VOID();
}
