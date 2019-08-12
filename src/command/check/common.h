/***********************************************************************************************************************************
Check Command Common
***********************************************************************************************************************************/
#ifndef COMMAND_CHECK_COMMON_H
#define COMMAND_CHECK_COMMON_H

#include "db/db.h"

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
void checkDbConfig(const unsigned int pgVersion, const Db *dbObject, const unsigned int dbIdx);

#endif
