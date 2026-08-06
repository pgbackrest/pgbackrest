/***********************************************************************************************************************************
Execute Process Extensions

Functions intended to simplify exec'ing and getting output. The core Exec object is efficient but it does not work well for the
requirements of the build, test, and doc which prefer ease of use.
***********************************************************************************************************************************/
#ifndef TEST_HARNESS_EXEC_H
#define TEST_HARNESS_EXEC_H

#include "common/exec.h"

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Execute a command similar to system() while also capturing output. Note that stderr is redirected to stdout.
typedef struct HrnExecOneExpectParam
{
    VAR_PARAM_HEADER;
    const String *shell;                                            // Shell command to use for exec (default is sh -c)
    int resultExpect;                                               // Expected result, if not 0
    TimeMSec timeout;                                               // Command timeout (default is ioTimeoutMs())
} HrnExecOneExpectParam;

#define hrnExecOneExpectP(command, ...)                                                                                            \
    hrnExecOneExpect(command, (HrnExecOneExpectParam){VAR_PARAM_INIT, __VA_ARGS__})

FN_EXTERN String *hrnExecOneExpect(const String *command, HrnExecOneExpectParam param);

#endif
