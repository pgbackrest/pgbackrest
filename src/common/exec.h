/***********************************************************************************************************************************
Execute Process

Executes a child process and allows the calling process to communicate with it using read/write io.

This object is specially tailored to implement the protocol layer and may or may not be generally applicable to general purpose
execution.
***********************************************************************************************************************************/
#ifndef COMMON_EXEC_H
#define COMMON_EXEC_H

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct Exec Exec;

#include "common/io/read.h"
#include "common/io/write.h"
#include "common/time.h"
#include "common/type/object.h"

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
Exec *execNew(const String *command, const StringList *param, const String *name, TimeMSec timeout);

/***********************************************************************************************************************************
Getters/Setters
***********************************************************************************************************************************/
typedef struct ExecPub
{
    IoRead *ioReadExec;                                             // Wrapper for file descriptor read interface
    IoWrite *ioWriteExec;                                           // Wrapper for file descriptor write interface
} ExecPub;

// Read interface
__attribute__((always_inline)) static inline IoRead *
execIoRead(Exec *const this)
{
    return THIS_PUB(Exec)->ioReadExec;
}

// Write interface
__attribute__((always_inline)) static inline IoWrite *
execIoWrite(Exec *const this)
{
    return THIS_PUB(Exec)->ioWriteExec;
}

// Exec MemContext
__attribute__((always_inline)) static inline MemContext *
execMemContext(Exec *const this)
{
    return objMemContext(this);
}

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Execute command
void execOpen(Exec *this);

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
__attribute__((always_inline)) static inline void
execFree(Exec *const this)
{
    objFree(this);
}

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_EXEC_TYPE                                                                                                     \
    Exec *
#define FUNCTION_LOG_EXEC_FORMAT(value, buffer, bufferSize)                                                                        \
    objToLog(value, "Exec", buffer, bufferSize)

#endif
