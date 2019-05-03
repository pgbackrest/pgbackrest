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
Constructor
***********************************************************************************************************************************/
ProtocolParallel *protocolParallelNew(TimeMSec timeout);

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
void protocolParallelClientAdd(ProtocolParallel *this, ProtocolClient *client);
void protocolParallelJobAdd(ProtocolParallel *this, ProtocolParallelJob *job);
unsigned int protocolParallelProcess(ProtocolParallel *this);

/***********************************************************************************************************************************
Getters
***********************************************************************************************************************************/
bool protocolParallelDone(const ProtocolParallel *this);
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
