/***********************************************************************************************************************************
Parse PostgreSQL Interface Yaml
***********************************************************************************************************************************/
#ifndef BUILD_POSTRES_PARSE_H
#define BUILD_POSTRES_PARSE_H

#include "build/common/string.h"

/***********************************************************************************************************************************
Types
***********************************************************************************************************************************/
typedef struct BldPgVersion
{
    const String *version;                                          // Version
    bool release;                                                   // Is this a released version?
} BldPgVersion;

typedef struct BldPg
{
    const List *pgList;                                            // Supported PostgreSQL versions
    const StringList *typeList;                                    // PostgreSQL interface types
    const StringList *defineList;                                  // PostgreSQL interface defines
    const StringList *functionList;                                // Functions defined by macros
} BldPg;

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Parse postgres.yaml
BldPg bldPgParse(const Storage *const storageRepo);

#endif
