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
#include "common/type/stringList.h"

/***********************************************************************************************************************************
Constructor
***********************************************************************************************************************************/
Exec *execNew(const String *command, const StringList *param, const String *name, TimeMSec timeout);

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
void execOpen(Exec *this);
void execRead(Exec *this, Buffer *buffer, bool block);
void execWrite(Exec *this, Buffer *buffer);

/***********************************************************************************************************************************
Getters
***********************************************************************************************************************************/
bool execEof(Exec *this);
IoRead *execIoRead(const Exec *this);
IoWrite *execIoWrite(const Exec *this);
MemContext *execMemContext(const Exec *this);

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
void execFree(Exec *this);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_DEBUG_EXEC_TYPE                                                                                                   \
    Exec *
#define FUNCTION_DEBUG_EXEC_FORMAT(value, buffer, bufferSize)                                                                      \
    objToLog(value, "Exec", buffer, bufferSize)

#endif
