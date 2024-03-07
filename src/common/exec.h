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
FN_EXTERN Exec *execNew(const String *command, const StringList *param, const String *name, TimeMSec timeout);

/***********************************************************************************************************************************
Getters/Setters
***********************************************************************************************************************************/
typedef struct ExecPub
{
    String *command;                                                // Command to execute
    IoRead *ioReadExec;                                             // Wrapper for file descriptor read interface
    IoWrite *ioWriteExec;                                           // Wrapper for file descriptor write interface
} ExecPub;

// Exec command
FN_INLINE_ALWAYS const String *
execCommand(const Exec *const this)
{
    return THIS_PUB(Exec)->command;
}

// Read interface
FN_INLINE_ALWAYS IoRead *
execIoRead(Exec *const this)
{
    return THIS_PUB(Exec)->ioReadExec;
}

// Write interface
FN_INLINE_ALWAYS IoWrite *
execIoWrite(Exec *const this)
{
    return THIS_PUB(Exec)->ioWriteExec;
}

// Exec MemContext
FN_INLINE_ALWAYS MemContext *
execMemContext(Exec *const this)
{
    return objMemContext(this);
}

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Execute command
FN_EXTERN void execOpen(Exec *this);

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
FN_INLINE_ALWAYS void
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
    objNameToLog(value, "Exec", buffer, bufferSize)

#endif
