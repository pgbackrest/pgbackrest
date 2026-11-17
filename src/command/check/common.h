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

// Validate that the archive and backup info files are at the same repository format
FN_EXTERN void checkStanzaInfoFormat(unsigned int repoIdx, const InfoPg *archiveInfoPg, const InfoPg *backupInfoPg);

// Validate the archive and backup info files
FN_EXTERN void checkStanzaInfo(unsigned int repoIdx, const InfoPg *archiveInfoPg, const InfoPg *backupInfoPg);

// Load and validate the database data of the info files against each other and the current database
FN_EXTERN void checkStanzaInfoPg(unsigned int repoIdx, const unsigned int pgVersion, const uint64_t pgSystemId);

#endif
