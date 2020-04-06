/***********************************************************************************************************************************
Protocol Parallel Job
***********************************************************************************************************************************/
#ifndef PROTOCOL_PARALLEL_JOB_H
#define PROTOCOL_PARALLEL_JOB_H

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
#define PROTOCOL_PARALLEL_JOB_TYPE                                  ProtocolParallelJob
#define PROTOCOL_PARALLEL_JOB_PREFIX                                protocolParallelJob

typedef struct ProtocolParallelJob ProtocolParallelJob;

/***********************************************************************************************************************************
Job state enum
***********************************************************************************************************************************/
typedef enum
{
    protocolParallelJobStatePending,
    protocolParallelJobStateRunning,
    protocolParallelJobStateDone,
} ProtocolParallelJobState;

#include "common/time.h"
#include "protocol/client.h"

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
ProtocolParallelJob *protocolParallelJobNew(const Variant *key, ProtocolCommand *command);

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Move to new parent mem context
ProtocolParallelJob *protocolParallelJobMove(ProtocolParallelJob *this, MemContext *parentNew);

/***********************************************************************************************************************************
Getters/Setters
***********************************************************************************************************************************/
// Job command
const ProtocolCommand *protocolParallelJobCommand(const ProtocolParallelJob *this);

// Job error
int protocolParallelJobErrorCode(const ProtocolParallelJob *this);
const String *protocolParallelJobErrorMessage(const ProtocolParallelJob *this);
void protocolParallelJobErrorSet(ProtocolParallelJob *this, int code, const String *message);

// Job key
const Variant *protocolParallelJobKey(const ProtocolParallelJob *this);

// Process Id
unsigned int protocolParallelJobProcessId(const ProtocolParallelJob *this);
void protocolParallelJobProcessIdSet(ProtocolParallelJob *this, unsigned int processId);

// Job result
const Variant *protocolParallelJobResult(const ProtocolParallelJob *this);
void protocolParallelJobResultSet(ProtocolParallelJob *this, const Variant *result);

// Job state
ProtocolParallelJobState protocolParallelJobState(const ProtocolParallelJob *this);
void protocolParallelJobStateSet(ProtocolParallelJob *this, ProtocolParallelJobState state);

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
void protocolParallelJobFree(ProtocolParallelJob *this);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
const char *protocolParallelJobToConstZ(ProtocolParallelJobState state);
String *protocolParallelJobToLog(const ProtocolParallelJob *this);

#define FUNCTION_LOG_PROTOCOL_PARALLEL_JOB_TYPE                                                                                    \
    ProtocolParallelJob *
#define FUNCTION_LOG_PROTOCOL_PARALLEL_JOB_FORMAT(value, buffer, bufferSize)                                                       \
    FUNCTION_LOG_STRING_OBJECT_FORMAT(value, protocolParallelJobToLog, buffer, bufferSize)

#endif
