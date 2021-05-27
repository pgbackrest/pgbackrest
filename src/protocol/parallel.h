/***********************************************************************************************************************************
Protocol Parallel Executor
***********************************************************************************************************************************/
#ifndef PROTOCOL_PARALLEL_H
#define PROTOCOL_PARALLEL_H

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct ProtocolParallel ProtocolParallel;

#include "common/time.h"
#include "common/type/object.h"
#include "protocol/client.h"
#include "protocol/parallelJob.h"

/***********************************************************************************************************************************
Job request callback

Called whenever a new job is required for processing.  If no more jobs are available then NULL is returned.  Note that NULL must be
returned to each clientIdx in case job distribution varies by clientIdx.
***********************************************************************************************************************************/
typedef ProtocolParallelJob *ParallelJobCallback(void *data, unsigned int clientIdx);

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
ProtocolParallel *protocolParallelNew(TimeMSec timeout, ParallelJobCallback *callbackFunction, void *callbackData);

/***********************************************************************************************************************************
Getters/Setters
***********************************************************************************************************************************/
// Are all jobs done?
bool protocolParallelDone(ProtocolParallel *this);

// Completed job result
ProtocolParallelJob *protocolParallelResult(ProtocolParallel *this);

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Add client
void protocolParallelClientAdd(ProtocolParallel *this, ProtocolClient *client);

// Process jobs
unsigned int protocolParallelProcess(ProtocolParallel *this);

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
__attribute__((always_inline)) static inline void
protocolParallelFree(ProtocolParallel *const this)
{
    objFree(this);
}

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
String *protocolParallelToLog(const ProtocolParallel *this);

#define FUNCTION_LOG_PROTOCOL_PARALLEL_TYPE                                                                                        \
    ProtocolParallel *
#define FUNCTION_LOG_PROTOCOL_PARALLEL_FORMAT(value, buffer, bufferSize)                                                           \
    FUNCTION_LOG_STRING_OBJECT_FORMAT(value, protocolParallelToLog, buffer, bufferSize)

#endif
