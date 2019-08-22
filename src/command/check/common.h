/***********************************************************************************************************************************
Check Command Common
***********************************************************************************************************************************/
#ifndef COMMAND_CHECK_COMMON_H
#define COMMAND_CHECK_COMMON_H

#include "common/type/string.h"
#include "info/infoPg.h"

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
void checkDbConfig(const unsigned int pgVersion, const unsigned int dbIdx, const unsigned int dbVersion, const String *dbPath);
void checkStanzaInfo(const InfoPgData *archiveInfo, const InfoPgData *backupInfo);

#endif
