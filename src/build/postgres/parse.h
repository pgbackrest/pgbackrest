/***********************************************************************************************************************************
Parse PostgreSQL Interface Yaml
***********************************************************************************************************************************/
#ifndef BUILD_POSTRES_PARSE_H
#define BUILD_POSTRES_PARSE_H

#include "common/type/string.h"

/***********************************************************************************************************************************
Types
***********************************************************************************************************************************/
typedef struct BldPgVersion
{
    const String *version;                                          // Version
    bool release;                                                   // Is this a released version?
} BldPgVersion;

typedef struct BldGpdbVersion
{
    const String *version;                                          // Version
    const String *pg_version;                                       // PostgreSQL version
} BldGpdbVersion;

typedef struct BldPg
{
    const List *pgList;                                            // Supported PostgreSQL versions
    const List *gpdbList;                                          // Supported GPDB versions
    const StringList *typeList;                                    // PostgreSQL interface types
    const StringList *defineList;                                  // PostgreSQL interface defines
    const StringList *functionList;                                // Functions defined by macros
} BldPg;

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Parse postgres.yaml and gpdb.yaml
BldPg bldPgParse(const Storage *const storageRepo);

#endif
