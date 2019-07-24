/***********************************************************************************************************************************
PostgreSQL Client

Connect to a PostgreSQL database and run queries.  This is not intended to be a general purpose client but is suitable for
pgBackRest's limited needs.  In particular, data type support is limited to text, int, and bool types so it may be necessary to add
casts to queries to output one of these days.
***********************************************************************************************************************************/
#ifndef POSTGRES_QUERY_H
#define POSTGRES_QUERY_H

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
#define PG_CLIENT_TYPE                                               PgClient
#define PG_CLIENT_PREFIX                                             pgClient

typedef struct PgClient PgClient;

/***********************************************************************************************************************************
Constructor
***********************************************************************************************************************************/
PgClient *pgClientNew(
    const String *host, const unsigned int port, const String *database, const String *user, const TimeMSec queryTimeout);

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
PgClient *pgClientOpen(PgClient *this);
VariantList *pgClientQuery(PgClient *this, const String *query);
void pgClientClose(PgClient *this);

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
void pgClientFree(PgClient *this);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
String *pgClientToLog(const PgClient *this);

#define FUNCTION_LOG_PG_CLIENT_TYPE                                                                                                \
    PgClient *
#define FUNCTION_LOG_PG_CLIENT_FORMAT(value, buffer, bufferSize)                                                                   \
    FUNCTION_LOG_STRING_OBJECT_FORMAT(value, pgClientToLog, buffer, bufferSize)

#endif
