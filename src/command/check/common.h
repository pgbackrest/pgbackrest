/***********************************************************************************************************************************
Check Command Common
***********************************************************************************************************************************/
#ifndef COMMAND_CHECK_COMMON_H
#define COMMAND_CHECK_COMMON_H

#include "common/type/string.h"
#include "db/db.h"
#include "info/infoPg.h"

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
void checkDbConfig(const unsigned int pgVersion, const unsigned int dbIdx, const Db *dbObject, bool isStandby);
void checkStanzaInfo(const InfoPgData *archiveInfo, const InfoPgData *backupInfo);
void checkStanzaInfoPg(
    const Storage *storage, const unsigned int pgVersion, const uint64_t pgSystemId, CipherType cipherType,
    const String *cipherPass);

#endif
