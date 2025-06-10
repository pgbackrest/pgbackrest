/***********************************************************************************************************************************
Protocol Parallel Job
***********************************************************************************************************************************/
#include "build.auto.h"

#include "common/debug.h"
#include "common/log.h"
#include "protocol/parallelJob.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct ProtocolParallelJob
{
    ProtocolParallelJobPub pub;                                     // Publicly accessible variables
};

/**********************************************************************************************************************************/
FN_EXTERN ProtocolParallelJob *
protocolParallelJobNew(const Variant *const key, const StringId command, PackWrite *const param)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(VARIANT, key);
        FUNCTION_LOG_PARAM(STRING_ID, command);
        FUNCTION_LOG_PARAM(PACK_WRITE, param);
    FUNCTION_LOG_END();

    OBJ_NEW_BEGIN(ProtocolParallelJob, .childQty = MEM_CONTEXT_QTY_MAX)
    {
        *this = (ProtocolParallelJob)
        {
            .pub =
            {
                .state = protocolParallelJobStatePending,
                .key = varDup(key),
                .command = command,
                .param = pckWriteMove(param, objMemContext(this)),
            },
        };
    }
    OBJ_NEW_END();

    FUNCTION_LOG_RETURN(PROTOCOL_PARALLEL_JOB, this);
}

/**********************************************************************************************************************************/
FN_EXTERN void
protocolParallelJobErrorSet(ProtocolParallelJob *const this, const int code, const String *const message)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(PROTOCOL_PARALLEL_JOB, this);
        FUNCTION_LOG_PARAM(INT, code);
        FUNCTION_LOG_PARAM(STRING, message);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(code != 0);
    ASSERT(message != NULL);

    MEM_CONTEXT_OBJ_BEGIN(this)
    {
        this->pub.code = code;
        this->pub.message = strDup(message);
    }
    MEM_CONTEXT_OBJ_END();

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
FN_EXTERN void
protocolParallelJobProcessIdSet(ProtocolParallelJob *const this, const unsigned int processId)
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
FN_EXTERN void
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
FN_EXTERN void
protocolParallelJobStateSet(ProtocolParallelJob *const this, const ProtocolParallelJobState state)
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

FN_EXTERN void
protocolParallelJobToLog(const ProtocolParallelJob *const this, StringStatic *const debugLog)
{
    strStcCat(debugLog, "{state: ");
    strStcResultSizeInc(debugLog, strIdToLog(protocolParallelJobState(this), strStcRemains(debugLog), strStcRemainsSize(debugLog)));

    strStcCat(debugLog, ", key: ");
    varToLog(protocolParallelJobKey(this), debugLog);

    strStcCat(debugLog, ", command: ");
    strStcResultSizeInc(
        debugLog, strIdToLog(protocolParallelJobCommand(this), strStcRemains(debugLog), strStcRemainsSize(debugLog)));

    strStcCat(debugLog, ", result: ");
    strStcResultSizeInc(
        debugLog,
        FUNCTION_LOG_OBJECT_FORMAT(
            protocolParallelJobResult(this), pckReadToLog, strStcRemains(debugLog), strStcRemainsSize(debugLog)));

    strStcFmt(debugLog, ", code: %d, message: ", protocolParallelJobErrorCode(this));

    strStcResultSizeInc(
        debugLog,
        FUNCTION_LOG_OBJECT_FORMAT(
            protocolParallelJobErrorMessage(this), strToLog, strStcRemains(debugLog), strStcRemainsSize(debugLog)));
    strStcCatChr(debugLog, '}');
}
