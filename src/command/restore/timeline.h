/***********************************************************************************************************************************
Timeline Management
***********************************************************************************************************************************/
#ifndef COMMAND_RESTORE_TIMELINE_H
#define COMMAND_RESTORE_TIMELINE_H

#include "storage/storage.h"

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Verify that target timeline is valid
FN_EXTERN void timelineVerify(
    const Storage *storageRepo, const String *archiveId, unsigned int pgVersion, unsigned int timelineCurrent,
    uint64_t lsnCurrent, const String *timelineTargetStr);

#endif
