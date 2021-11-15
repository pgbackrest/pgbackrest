/***********************************************************************************************************************************
PostgreSQL Client

Connect to a PostgreSQL database and run queries.  This is not intended to be a general purpose client but is suitable for
pgBackRest's limited needs.  In particular, data type support is limited to text, int, and bool types so it may be necessary to add
casts to queries to output one of these types.
***********************************************************************************************************************************/
#ifndef POSTGRES_QUERY_H
#define POSTGRES_QUERY_H

#include "common/type/object.h"
#include "common/type/string.h"
#include "common/type/variantList.h"
#include "common/time.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct PgClient PgClient;

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
PgClient *pgClientNew(
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
__attribute__((always_inline)) static inline const String *
pgClientHost(const PgClient *const this)
{
    return THIS_PUB(PgClient)->host;
}

// Pg port
__attribute__((always_inline)) static inline unsigned int
pgClientPort(const PgClient *const this)
{
    return THIS_PUB(PgClient)->port;
}

// Pg database
__attribute__((always_inline)) static inline const String *
pgClientDatabase(const PgClient *const this)
{
    return THIS_PUB(PgClient)->database;
}

// Pg user
__attribute__((always_inline)) static inline const String *
pgClientUser(const PgClient *const this)
{
    return THIS_PUB(PgClient)->user;
}

// Timeout for statements/queries
__attribute__((always_inline)) static inline TimeMSec
pgClientTimeout(const PgClient *const this)
{
    return THIS_PUB(PgClient)->timeout;
}

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Open connection to PostgreSQL
PgClient *pgClientOpen(PgClient *this);

// Move to a new parent mem context
__attribute__((always_inline)) static inline PgClient *
pgClientMove(PgClient *const this, MemContext *const parentNew)
{
    return objMove(this, parentNew);
}

// Execute a query and return results
VariantList *pgClientQuery(PgClient *this, const String *query);

// Close connection to PostgreSQL
void pgClientClose(PgClient *this);

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
__attribute__((always_inline)) static inline void
pgClientFree(PgClient *const this)
{
    objFree(this);
}

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
String *pgClientToLog(const PgClient *this);

#define FUNCTION_LOG_PG_CLIENT_TYPE                                                                                                \
    PgClient *
#define FUNCTION_LOG_PG_CLIENT_FORMAT(value, buffer, bufferSize)                                                                   \
    FUNCTION_LOG_STRING_OBJECT_FORMAT(value, pgClientToLog, buffer, bufferSize)

#endif
