/***********************************************************************************************************************************
Protocol Parallel Executor
***********************************************************************************************************************************/
#ifndef PROTOCOL_PARALLEL_H
#define PROTOCOL_PARALLEL_H

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
#define PROTOCOL_PARALLEL_TYPE                                      ProtocolParallel
#define PROTOCOL_PARALLEL_PREFIX                                    protocolParallel

typedef struct ProtocolParallel ProtocolParallel;

#include "common/time.h"
#include "protocol/client.h"
#include "protocol/parallelJob.h"

/***********************************************************************************************************************************
Job request callback

Called whenever a new job is required for processing.  If no more jobs are available then NULL is returned.  Note that NULL must be
returned to each clientIdx in case job distribution varies by clientIdx.
***********************************************************************************************************************************/
typedef ProtocolParallelJob *ParallelJobCallback(void *data, unsigned int clientIdx);

/***********************************************************************************************************************************
Constructor
***********************************************************************************************************************************/
ProtocolParallel *protocolParallelNew(TimeMSec timeout, ParallelJobCallback *callbackFunction, void *callbackData);

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Add client
void protocolParallelClientAdd(ProtocolParallel *this, ProtocolClient *client);

// Process jobs
unsigned int protocolParallelProcess(ProtocolParallel *this);

/***********************************************************************************************************************************
Getters
***********************************************************************************************************************************/
// Are all jobs done?
bool protocolParallelDone(const ProtocolParallel *this);

// Completed job result
ProtocolParallelJob *protocolParallelResult(ProtocolParallel *this);

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
void protocolParallelFree(ProtocolParallel *this);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
String *protocolParallelToLog(const ProtocolParallel *this);

#define FUNCTION_LOG_PROTOCOL_PARALLEL_TYPE                                                                                        \
    ProtocolParallel *
#define FUNCTION_LOG_PROTOCOL_PARALLEL_FORMAT(value, buffer, bufferSize)                                                           \
    FUNCTION_LOG_STRING_OBJECT_FORMAT(value, protocolParallelToLog, buffer, bufferSize)

#endif
