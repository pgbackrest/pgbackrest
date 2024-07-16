/***********************************************************************************************************************************
Execute Process Extensions
***********************************************************************************************************************************/
// Include core module
#include "common/exec.c"

#include "build/common/exec.h"
#include "common/io/fd.h"

/**********************************************************************************************************************************/
static String *
execProcess(Exec *const this, const ExecOneParam param)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(EXEC, this);
        FUNCTION_LOG_PARAM(INT, param.resultExpect);
    FUNCTION_LOG_END();

    String *const result = strNew();

    MEM_CONTEXT_TEMP_BEGIN()
    {
        int processStatus;
        int processResult;

        ioReadOpen(this->ioReadFd);
        strCat(result, strNewBuf(ioReadBuf(this->ioReadFd)));

        THROW_ON_SYS_ERROR(
            (processResult = waitpid(this->processId, &processStatus, 0)) == -1, ExecuteError,
            "unable to wait on child process");

        // Clear the process id so we don't try to wait for this process on free
        this->processId = 0;

        // If the process exited normally but without a success status
        if (WIFEXITED(processStatus))
        {
            if (WEXITSTATUS(processStatus) != param.resultExpect)
                execCheckStatusError(this, processStatus, strTrim(result));
        }
        // Else if the process did not exit normally then it must have been a signal
        else
            execCheckSignalError(this, processStatus);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(STRING, result);
}

/**********************************************************************************************************************************/
FN_EXTERN String *
execOne(const String *const command, const ExecOneParam param)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STRING, command);
        FUNCTION_LOG_PARAM(STRING, param.shell);
        FUNCTION_LOG_PARAM(INT, param.resultExpect);
    FUNCTION_LOG_END();

    String *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        const StringList *const shellList = strLstNewSplitZ(param.shell != NULL ? param.shell : STRDEF("sh -c"), " ");
        StringList *const paramList = strLstNew();

        ASSERT(strLstSize(shellList) != 0);

        for (unsigned int shellIdx = 1; shellIdx < strLstSize(shellList); shellIdx++)
            strLstAdd(paramList, strLstGet(shellList, shellIdx));

        strLstAddFmt(paramList, "%s 2>&1", strZ(command));
        strLstAddZ(paramList, "2>&1");

        Exec *const exec = execNew(strLstGet(shellList, 0), paramList, command, ioTimeoutMs());

        execOpen(exec);

        MEM_CONTEXT_PRIOR_BEGIN()
        {
            result = execProcess(exec, param);
        }
        MEM_CONTEXT_PRIOR_END();
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(STRING, result);
}
