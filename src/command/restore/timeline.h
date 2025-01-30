/***********************************************************************************************************************************
Timeline Management
***********************************************************************************************************************************/
#ifndef COMMAND_RESTORE_TIMELINE_H
#define COMMAND_RESTORE_TIMELINE_H

#include "common/crypto/common.h"
#include "storage/storage.h"

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Verify that target timeline is valid for a backup
FN_EXTERN void timelineVerify(
    const Storage *storageRepo, const String *archiveId, unsigned int pgVersion, unsigned int timelineBackup,
    uint64_t lsnBackup, StringId recoveryType, const String *timelineTargetStr, CipherType cipherType, const String *cipherPass);

#endif
