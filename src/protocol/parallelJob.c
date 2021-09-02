/***********************************************************************************************************************************
Protocol Parallel Job
***********************************************************************************************************************************/
#include "build.auto.h"

#include "common/debug.h"
#include "common/log.h"
#include "protocol/command.h"
#include "protocol/parallelJob.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct ProtocolParallelJob
{
    ProtocolParallelJobPub pub;                                     // Publicly accessible variables
};

/**********************************************************************************************************************************/
ProtocolParallelJob *
protocolParallelJobNew(const Variant *key, ProtocolCommand *command)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(VARIANT, key);
        FUNCTION_LOG_PARAM(PROTOCOL_COMMAND, command);
    FUNCTION_LOG_END();

    ProtocolParallelJob *this = NULL;

    OBJ_NEW_BEGIN(ProtocolParallelJob)
    {
        this = OBJ_NEW_ALLOC();

        *this = (ProtocolParallelJob)
        {
            .pub =
            {
                .state = protocolParallelJobStatePending,
                .key = varDup(key),
            },
        };

        this->pub.command = protocolCommandMove(command, objMemContext(this));
    }
    OBJ_NEW_END();

    FUNCTION_LOG_RETURN(PROTOCOL_PARALLEL_JOB, this);
}

/**********************************************************************************************************************************/
void
protocolParallelJobErrorSet(ProtocolParallelJob *this, int code, const String *message)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(PROTOCOL_PARALLEL_JOB, this);
        FUNCTION_LOG_PARAM(INT, code);
        FUNCTION_LOG_PARAM(STRING, message);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(code != 0);
    ASSERT(message != NULL);

    MEM_CONTEXT_BEGIN(objMemContext(this))
    {
        this->pub.code = code;
        this->pub.message = strDup(message);
    }
    MEM_CONTEXT_END();

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
void
protocolParallelJobProcessIdSet(ProtocolParallelJob *this, unsigned int processId)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(PROTOCOL_PARALLEL_JOB, this);
        FUNCTION_LOG_PARAM(UINT, processId);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(processId > 0);

    this->pub.processId = processId;

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
void
protocolParallelJobResultSet(ProtocolParallelJob *const this, PackRead *const result)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(PROTOCOL_PARALLEL_JOB, this);
        FUNCTION_LOG_PARAM(PACK_READ, result);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(protocolParallelJobErrorCode(this) == 0);

    this->pub.result = pckReadMove(result, objMemContext(this));

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
void
protocolParallelJobStateSet(ProtocolParallelJob *this, ProtocolParallelJobState state)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(PROTOCOL_PARALLEL_JOB, this);
        FUNCTION_LOG_PARAM(STRING_ID, state);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    if (this->pub.state == protocolParallelJobStatePending && state == protocolParallelJobStateRunning)
        this->pub.state = protocolParallelJobStateRunning;
    else if (this->pub.state == protocolParallelJobStateRunning && state == protocolParallelJobStateDone)
        this->pub.state = protocolParallelJobStateDone;
    else
    {
        THROW_FMT(
            AssertError, "invalid state transition from '%s' to '%s'", strZ(strIdToStr(protocolParallelJobState(this))),
            strZ(strIdToStr(state)));
    }

    FUNCTION_LOG_RETURN_VOID();
}

String *
protocolParallelJobToLog(const ProtocolParallelJob *this)
{
    return strNewFmt(
        "{state: %s, key: %s, command: %s, code: %d, message: %s, result: %s}",
        strZ(strIdToStr(protocolParallelJobState(this))), strZ(varToLog(protocolParallelJobKey(this))),
        strZ(protocolCommandToLog(protocolParallelJobCommand(this))), protocolParallelJobErrorCode(this),
        strZ(strToLog(protocolParallelJobErrorMessage(this))),
        protocolParallelJobResult(this) == NULL ? NULL_Z : strZ(pckReadToLog(protocolParallelJobResult(this))));
}
