/***********************************************************************************************************************************
Systemd Harness
***********************************************************************************************************************************/
#include <build.h>

#include <string.h>

#include "common/debug.h"
#include "common/log.h"

#include "common/harnessDebug.h"
#include "common/harnessSystemd.h"

static struct
{
    bool ready;
    bool stopping;
} hrnSystemdStatic;

/***********************************************************************************************************************************
Shim sd_notify to track notify state
***********************************************************************************************************************************/
#ifdef HAVE_LIBSYSTEMD

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
        ASSERT(!hrnSystemdStatic.stopping);
        hrnSystemdStatic.ready = true;
    }
    else if (strcmp(state, "STOPPING=1") == 0)
    {
        ASSERT(hrnSystemdStatic.ready);
        hrnSystemdStatic.stopping = true;
    }
    else
        THROW_FMT(AssertError, "unknown sd_notify state '%s'", state);

    FUNCTION_HARNESS_RETURN(INT, 1);
}

#endif

/**********************************************************************************************************************************/
void
hrnSystemDCheck(void)
{
    FUNCTION_HARNESS_VOID();

    ASSERT(hrnSystemdStatic.ready && hrnSystemdStatic.stopping);

    FUNCTION_HARNESS_RETURN_VOID();
}
