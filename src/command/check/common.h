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
// Check the database path and version are configured correctly
FN_EXTERN void checkDbConfig(const unsigned int pgVersion, const unsigned int pgIdx, const Db *dbObject, bool isStandby);

// Validate the archive and backup info files
FN_EXTERN void checkStanzaInfo(const InfoPgData *archiveInfo, const InfoPgData *backupInfo);

// Load and validate the database data of the info files against each other and the current database
FN_EXTERN void checkStanzaInfoPg(
    const Storage *storage, const unsigned int pgVersion, const uint64_t pgSystemId, CipherType cipherType,
    const String *cipherPass);

#endif
