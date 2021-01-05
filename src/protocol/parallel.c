/***********************************************************************************************************************************
Protocol Parallel Executor
***********************************************************************************************************************************/
#include "build.auto.h"

#include <string.h>
#include <sys/select.h>

#include "common/debug.h"
#include "common/log.h"
#include "common/memContext.h"
#include "common/type/json.h"
#include "common/type/keyValue.h"
#include "common/type/list.h"
#include "common/type/object.h"
#include "protocol/command.h"
#include "protocol/helper.h"
#include "protocol/parallel.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct ProtocolParallel
{
    MemContext *memContext;
    TimeMSec timeout;                                               // Max time to wait for jobs before returning
    ParallelJobCallback *callbackFunction;                          // Function to get new jobs
    void *callbackData;                                             // Data to pass to callback function

    List *clientList;                                               // List of clients to process jobs
    List *jobList;                                                  // List of jobs to be processed

    ProtocolParallelJob **clientJobList;                            // Jobs being processing by each client

    ProtocolParallelJobState state;                                 // Overall state of job processing
};

OBJECT_DEFINE_FREE(PROTOCOL_PARALLEL);

/**********************************************************************************************************************************/
ProtocolParallel *
protocolParallelNew(TimeMSec timeout, ParallelJobCallback *callbackFunction, void *callbackData)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(UINT64, timeout);
        FUNCTION_LOG_PARAM(FUNCTIONP, callbackFunction);
        FUNCTION_LOG_PARAM_P(VOID, callbackData);
    FUNCTION_LOG_END();

    ASSERT(callbackFunction != NULL);
    ASSERT(callbackData != NULL);

    ProtocolParallel *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("ProtocolParallel")
    {
        this = memNew(sizeof(ProtocolParallel));

        *this = (ProtocolParallel)
        {
            .memContext = MEM_CONTEXT_NEW(),
            .timeout = timeout,
            .callbackFunction = callbackFunction,
            .callbackData = callbackData,
            .clientList = lstNewP(sizeof(ProtocolClient *)),
            .jobList = lstNewP(sizeof(ProtocolParallelJob *)),
            .state = protocolParallelJobStatePending,
        };
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_LOG_RETURN(PROTOCOL_PARALLEL, this);
}

/**********************************************************************************************************************************/
void
protocolParallelClientAdd(ProtocolParallel *this, ProtocolClient *client)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(PROTOCOL_PARALLEL, this);
        FUNCTION_LOG_PARAM(PROTOCOL_CLIENT, client);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(client != NULL);
    ASSERT(this->state == protocolParallelJobStatePending);

    if (ioReadFd(protocolClientIoRead(client)) == -1)
        THROW(AssertError, "client with read fd is required");

    lstAdd(this->clientList, &client);

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
unsigned int
protocolParallelProcess(ProtocolParallel *this)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(PROTOCOL_PARALLEL, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(this->state != protocolParallelJobStateDone);

    unsigned int result = 0;

    // If called for the first time, initialize processing
    if (this->state == protocolParallelJobStatePending)
    {
        MEM_CONTEXT_BEGIN(this->memContext)
        {
            this->clientJobList = memNewPtrArray(lstSize(this->clientList));
        }
        MEM_CONTEXT_END();

        this->state = protocolParallelJobStateRunning;
    }

    // Initialize the file descriptor set used for select
    fd_set selectSet;
    FD_ZERO(&selectSet);
    int fdMax = -1;

    // Find clients that are running jobs
    unsigned int clientRunningTotal = 0;

    for (unsigned int clientIdx = 0; clientIdx < lstSize(this->clientList); clientIdx++)
    {
        if (this->clientJobList[clientIdx] != NULL)
        {
            int fd = ioReadFd(protocolClientIoRead(*(ProtocolClient **)lstGet(this->clientList, clientIdx)));
            FD_SET((unsigned int)fd, &selectSet);

            // Find the max file descriptor needed for select()
            MAX_ASSIGN(fdMax, fd);

            clientRunningTotal++;
        }
    }

    // If clients are running then wait for one to finish
    if (clientRunningTotal > 0)
    {
        // Initialize timeout struct used for select.  Recreate this structure each time since Linux (at least) will modify it.
        struct timeval timeoutSelect;
        timeoutSelect.tv_sec = (time_t)(this->timeout / MSEC_PER_SEC);
        timeoutSelect.tv_usec = (time_t)(this->timeout % MSEC_PER_SEC * 1000);

        // Determine if there is data to be read
        int completed = select(fdMax + 1, &selectSet, NULL, NULL, &timeoutSelect);
        THROW_ON_SYS_ERROR(completed == -1, AssertError, "unable to select from parallel client(s)");

        // If any jobs have completed then get the results
        if (completed > 0)
        {
            for (unsigned int clientIdx = 0; clientIdx < lstSize(this->clientList); clientIdx++)
            {
                ProtocolParallelJob *job = this->clientJobList[clientIdx];

                if (job != NULL &&
                    FD_ISSET(
                        (unsigned int)ioReadFd(protocolClientIoRead(*(ProtocolClient **)lstGet(this->clientList, clientIdx))),
                        &selectSet))
                {
                    MEM_CONTEXT_TEMP_BEGIN()
                    {
                        TRY_BEGIN()
                        {
                            protocolParallelJobResultSet(
                                job, protocolClientReadOutputVar(*(ProtocolClient **)lstGet(this->clientList, clientIdx), true));
                        }
                        CATCH_ANY()
                        {
                            protocolParallelJobErrorSet(job, errorCode(), STR(errorMessage()));
                        }
                        TRY_END();

                        protocolParallelJobStateSet(job, protocolParallelJobStateDone);
                        this->clientJobList[clientIdx] = NULL;
                    }
                    MEM_CONTEXT_TEMP_END();
                }
            }

            result = (unsigned int)completed;
        }
    }

    // Find new jobs to be run
    for (unsigned int clientIdx = 0; clientIdx < lstSize(this->clientList); clientIdx++)
    {
        // If nothing is running for this client
        if (this->clientJobList[clientIdx] == NULL)
        {
            // Get a new job
            ProtocolParallelJob *job = NULL;

            MEM_CONTEXT_BEGIN(lstMemContext(this->jobList))
            {
                job = this->callbackFunction(this->callbackData, clientIdx);
            }
            MEM_CONTEXT_END();

            // If a new job was found
            if (job != NULL)
            {
                // Add to the job list
                lstAdd(this->jobList, &job);

                // Send the job to the client
                protocolCommandWrite(
                    protocolParallelJobCommand(job),
                    protocolClientIoWrite(*(ProtocolClient **)lstGet(this->clientList, clientIdx)));

                // Set client id and running state
                protocolParallelJobProcessIdSet(job, clientIdx + 1);
                protocolParallelJobStateSet(job, protocolParallelJobStateRunning);
                this->clientJobList[clientIdx] = job;
            }
            // Else no more jobs for this client so free it
            else
                protocolLocalFree(clientIdx + 1);
        }
    }

    FUNCTION_LOG_RETURN(UINT, result);
}

/**********************************************************************************************************************************/
ProtocolParallelJob *
protocolParallelResult(ProtocolParallel *this)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(PROTOCOL_PARALLEL, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(this->state == protocolParallelJobStateRunning);

    ProtocolParallelJob *result = NULL;

    // Find the next completed job
    for (unsigned int jobIdx = 0; jobIdx < lstSize(this->jobList); jobIdx++)
    {
        ProtocolParallelJob *job = *(ProtocolParallelJob **)lstGet(this->jobList, jobIdx);

        if (protocolParallelJobState(job) == protocolParallelJobStateDone)
        {
            result = protocolParallelJobMove(job, memContextCurrent());
            lstRemoveIdx(this->jobList, jobIdx);
            break;
        }
    }

    FUNCTION_LOG_RETURN(PROTOCOL_PARALLEL_JOB, result);
}

/**********************************************************************************************************************************/
bool
protocolParallelDone(ProtocolParallel *this)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(PROTOCOL_PARALLEL, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(this->state != protocolParallelJobStatePending);

    // If there are no jobs left then we are done
    if (this->state != protocolParallelJobStateDone && lstSize(this->jobList) == 0)
        this->state = protocolParallelJobStateDone;

    FUNCTION_LOG_RETURN(BOOL, this->state == protocolParallelJobStateDone);
}

/**********************************************************************************************************************************/
String *
protocolParallelToLog(const ProtocolParallel *this)
{
    return strNewFmt(
        "{state: %s, clientTotal: %u, jobTotal: %u}", protocolParallelJobToConstZ(this->state), lstSize(this->clientList),
        lstSize(this->jobList));
}
