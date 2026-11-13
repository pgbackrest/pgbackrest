/***********************************************************************************************************************************
Info Helper Internal
***********************************************************************************************************************************/
#ifndef INFO_HELPER_INTERN_H
#define INFO_HELPER_INTERN_H

#include "info/helper.h"

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Add new archive info to the cache for a repo. Only for archive info that does not exist in the repo yet, i.e. stanza-create.
FN_EXTERN void infoArchiveRepoIdxSet(unsigned int repoIdx, InfoArchive *infoArchive);

// Add new backup info to the cache for a repo. Only for backup info that does not exist in the repo yet, i.e. stanza-create.
FN_EXTERN void infoBackupRepoIdxSet(unsigned int repoIdx, InfoBackup *infoBackup);

#endif
