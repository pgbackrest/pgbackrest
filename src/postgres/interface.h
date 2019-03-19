/***********************************************************************************************************************************
PostgreSQL Interface
***********************************************************************************************************************************/
#ifndef POSTGRES_INTERFACE_H
#define POSTGRES_INTERFACE_H

#include <stdint.h>
#include <sys/types.h>

#include "common/type/string.h"

/***********************************************************************************************************************************
Defines for various Postgres paths and files
***********************************************************************************************************************************/
#define PG_FILE_PGCONTROL                                           "pg_control"

#define PG_PATH_GLOBAL                                              "global"

/***********************************************************************************************************************************
PostgreSQL Control File Info
***********************************************************************************************************************************/
typedef struct PgControl
{
    unsigned int version;
    uint64_t systemId;

    uint32_t controlVersion;
    uint32_t catalogVersion;

    unsigned int pageSize;
    unsigned int walSegmentSize;

    bool pageChecksum;
} PgControl;

/***********************************************************************************************************************************
PostgreSQL WAL Info
***********************************************************************************************************************************/
typedef struct PgWal
{
    unsigned int version;
    uint64_t systemId;
} PgWal;

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
PgControl pgControlFromFile(const String *pgPath);
PgControl pgControlFromBuffer(const Buffer *controlFile);
unsigned int pgVersionFromStr(const String *version);
String *pgVersionToStr(unsigned int version);

PgWal pgWalFromFile(const String *walFile);
PgWal pgWalFromBuffer(const Buffer *walBuffer);

/***********************************************************************************************************************************
Test Functions
***********************************************************************************************************************************/
#ifdef DEBUG
    Buffer *pgControlTestToBuffer(PgControl pgControl);
    void pgWalTestToBuffer(PgWal pgWal, Buffer *walBuffer);
#endif

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
String *pgControlToLog(const PgControl *pgControl);
String *pgWalToLog(const PgWal *pgWal);

#define FUNCTION_LOG_PG_CONTROL_TYPE                                                                                               \
    PgControl
#define FUNCTION_LOG_PG_CONTROL_FORMAT(value, buffer, bufferSize)                                                                  \
    FUNCTION_LOG_STRING_OBJECT_FORMAT(&value, pgControlToLog, buffer, bufferSize)

#define FUNCTION_LOG_PG_WAL_TYPE                                                                                                   \
    PgWal
#define FUNCTION_LOG_PG_WAL_FORMAT(value, buffer, bufferSize)                                                                      \
    FUNCTION_LOG_STRING_OBJECT_FORMAT(&value, pgWalToLog, buffer, bufferSize)

#endif
