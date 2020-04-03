/***********************************************************************************************************************************
Protocol Parallel Job
***********************************************************************************************************************************/
#include "build.auto.h"

#include "common/debug.h"
#include "common/log.h"
#include "common/memContext.h"
#include "common/type/object.h"
#include "protocol/command.h"
#include "protocol/parallelJob.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct ProtocolParallelJob
{
    MemContext *memContext;                                         // Job mem context
    ProtocolParallelJobState state;                                 // Current state of the job

    const Variant *key;                                             // Unique key used to identify the job
    const ProtocolCommand *command;                                 // Command to be executed

    unsigned int processId;                                         // Process that executed this job
    int code;                                                       // Non-zero result indicates an error
    String *message;                                                // Message if there was a error
    const Variant *result;                                          // Result if job was successful
};

OBJECT_DEFINE_MOVE(PROTOCOL_PARALLEL_JOB);
OBJECT_DEFINE_FREE(PROTOCOL_PARALLEL_JOB);

/**********************************************************************************************************************************/
ProtocolParallelJob *
protocolParallelJobNew(const Variant *key, ProtocolCommand *command)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(VARIANT, key);
        FUNCTION_LOG_PARAM(PROTOCOL_COMMAND, command);
    FUNCTION_LOG_END();

    ProtocolParallelJob *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("ProtocolParallelJob")
    {
        this = memNew(sizeof(ProtocolParallelJob));

        *this = (ProtocolParallelJob)
        {
            .memContext = memContextCurrent(),
            .state = protocolParallelJobStatePending,
            .key = varDup(key),
        };

        this->command = protocolCommandMove(command, this->memContext);
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_LOG_RETURN(PROTOCOL_PARALLEL_JOB, this);
}

/**********************************************************************************************************************************/
const ProtocolCommand *
protocolParallelJobCommand(const ProtocolParallelJob *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PROTOCOL_PARALLEL_JOB, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->command);
}

/**********************************************************************************************************************************/
int
protocolParallelJobErrorCode(const ProtocolParallelJob *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PROTOCOL_PARALLEL_JOB, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->code);
}

const String *
protocolParallelJobErrorMessage(const ProtocolParallelJob *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PROTOCOL_PARALLEL_JOB, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->message);
}

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

    MEM_CONTEXT_BEGIN(this->memContext)
    {
        this->code = code;
        this->message = strDup(message);
    }
    MEM_CONTEXT_END();

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
const Variant *
protocolParallelJobKey(const ProtocolParallelJob *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PROTOCOL_PARALLEL_JOB, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->key);
}

/**********************************************************************************************************************************/
unsigned int
protocolParallelJobProcessId(const ProtocolParallelJob *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PROTOCOL_PARALLEL_JOB, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->processId);
}

void
protocolParallelJobProcessIdSet(ProtocolParallelJob *this, unsigned int processId)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(PROTOCOL_PARALLEL_JOB, this);
        FUNCTION_LOG_PARAM(UINT, processId);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(processId > 0);

    this ->processId = processId;

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
const Variant *
protocolParallelJobResult(const ProtocolParallelJob *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PROTOCOL_PARALLEL_JOB, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->result);
}

void
protocolParallelJobResultSet(ProtocolParallelJob *this, const Variant *result)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(PROTOCOL_PARALLEL_JOB, this);
        FUNCTION_LOG_PARAM(VARIANT, result);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(this->code == 0);

    MEM_CONTEXT_BEGIN(this->memContext)
    {
        this->result = varDup(result);
    }
    MEM_CONTEXT_END();

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
ProtocolParallelJobState
protocolParallelJobState(const ProtocolParallelJob *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PROTOCOL_PARALLEL_JOB, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->state);
}

void
protocolParallelJobStateSet(ProtocolParallelJob *this, ProtocolParallelJobState state)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(PROTOCOL_PARALLEL_JOB, this);
        FUNCTION_LOG_PARAM(ENUM, state);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    if (this->state == protocolParallelJobStatePending && state == protocolParallelJobStateRunning)
        this->state = protocolParallelJobStateRunning;
    else if (this->state == protocolParallelJobStateRunning && state == protocolParallelJobStateDone)
        this->state = protocolParallelJobStateDone;
    else
    {
        THROW_FMT(
            AssertError, "invalid state transition from '%s' to '%s'", protocolParallelJobToConstZ(this->state),
            protocolParallelJobToConstZ(state));
    }

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
const char *
protocolParallelJobToConstZ(ProtocolParallelJobState state)
{
    const char *result = NULL;

    switch (state)
    {
        case protocolParallelJobStatePending:
        {
            result = "pending";
            break;
        }

        case protocolParallelJobStateRunning:
        {
            result = "running";
            break;
        }

        case protocolParallelJobStateDone:
        {
            result = "done";
            break;
        }
    }

    return result;
}

String *
protocolParallelJobToLog(const ProtocolParallelJob *this)
{
    return strNewFmt(
        "{state: %s, key: %s, command: %s, code: %d, message: %s, result: %s}", protocolParallelJobToConstZ(this->state),
        strPtr(varToLog(this->key)), strPtr(protocolCommandToLog(this->command)), this->code, strPtr(strToLog(this->message)),
        strPtr(varToLog(this->result)));
}
