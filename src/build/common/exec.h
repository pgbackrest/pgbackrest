/***********************************************************************************************************************************
Execute Process Extensions

Functions intended to simplify exec'ing and getting output. The core Exec object is efficient but it does not work well for the
requirements of the build, test, and doc which prefer ease of use.
***********************************************************************************************************************************/
#ifndef BUILD_COMMON_EXEC_H
#define BUILD_COMMON_EXEC_H

#include "common/exec.h"

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Execute a command similar to system() while also capturing output. Note that stderr is redirected to stdout.
typedef struct ExecOneParam
{
    VAR_PARAM_HEADER;
    const String *shell;                                            // Shell command to use for exec (default is sh -c)
    int resultExpect;                                               // Expected result, if not 0
} ExecOneParam;

#define execOneP(command, ...)                                                                                                     \
    execOne(command, (ExecOneParam){VAR_PARAM_INIT, __VA_ARGS__})

FN_EXTERN String *execOne(const String *command, ExecOneParam param);

#endif
