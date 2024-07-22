/***********************************************************************************************************************************
PostgreSQL Client

Connect to a PostgreSQL database and run queries. This is not intended to be a general purpose client but is suitable for
pgBackRest's limited needs. In particular, data type support is limited to text, int, and bool types so it may be necessary to add
casts to queries to output one of these types.
***********************************************************************************************************************************/
#ifndef POSTGRES_QUERY_H
#define POSTGRES_QUERY_H

#include "common/time.h"
#include "common/type/object.h"
#include "common/type/pack.h"
#include "common/type/string.h"

/***********************************************************************************************************************************
Query result types
***********************************************************************************************************************************/
typedef enum
{
    pgClientQueryResultAny = STRID5("any", 0x65c10),                // Any number of rows/columns expected (even none)
    pgClientQueryResultRow = STRID5("row", 0x5df20),                // One row expected
    pgClientQueryResultColumn = STRID5("column", 0x1cdab1e30),      // One row/column expected
    pgClientQueryResultNone = STRID5("none", 0x2b9ee0),             // No rows expected
} PgClientQueryResult;

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct PgClient PgClient;

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
FN_EXTERN PgClient *pgClientNew(
    const String *host, const unsigned int port, const String *database, const String *user, const TimeMSec timeout);

/***********************************************************************************************************************************
Getters/Setters
***********************************************************************************************************************************/
typedef struct PgClientPub
{
    const String *host;                                             // Pg host
    unsigned int port;                                              // Pg port
    const String *database;                                         // Pg database
    const String *user;                                             // Pg user
    TimeMSec timeout;                                               // Timeout for statements/queries
} PgClientPub;

// Pg host
FN_INLINE_ALWAYS const String *
pgClientHost(const PgClient *const this)
{
    return THIS_PUB(PgClient)->host;
}

// Pg port
FN_INLINE_ALWAYS unsigned int
pgClientPort(const PgClient *const this)
{
    return THIS_PUB(PgClient)->port;
}

// Pg database
FN_INLINE_ALWAYS const String *
pgClientDatabase(const PgClient *const this)
{
    return THIS_PUB(PgClient)->database;
}

// Pg user
FN_INLINE_ALWAYS const String *
pgClientUser(const PgClient *const this)
{
    return THIS_PUB(PgClient)->user;
}

// Timeout for statements/queries
FN_INLINE_ALWAYS TimeMSec
pgClientTimeout(const PgClient *const this)
{
    return THIS_PUB(PgClient)->timeout;
}

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Open connection to PostgreSQL
FN_EXTERN PgClient *pgClientOpen(PgClient *this);

// Move to a new parent mem context
FN_INLINE_ALWAYS PgClient *
pgClientMove(PgClient *const this, MemContext *const parentNew)
{
    return objMove(this, parentNew);
}

// Execute a query and return results
FN_EXTERN Pack *pgClientQuery(PgClient *this, const String *query, PgClientQueryResult resultType);

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
FN_INLINE_ALWAYS void
pgClientFree(PgClient *const this)
{
    objFree(this);
}

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
FN_EXTERN void pgClientToLog(const PgClient *this, StringStatic *debugLog);

#define FUNCTION_LOG_PG_CLIENT_TYPE                                                                                                \
    PgClient *
#define FUNCTION_LOG_PG_CLIENT_FORMAT(value, buffer, bufferSize)                                                                   \
    FUNCTION_LOG_OBJECT_FORMAT(value, pgClientToLog, buffer, bufferSize)

#endif
