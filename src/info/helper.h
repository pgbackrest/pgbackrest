/***********************************************************************************************************************************
Info Helper

Archive and backup info for each repo, loaded on first use and cached until the configuration is loaded again. The cached info is
saved with this helper.
***********************************************************************************************************************************/
#ifndef INFO_HELPER_H
#define INFO_HELPER_H

#include "info/infoArchive.h"
#include "info/infoBackup.h"

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Archive info for a repo
FN_EXTERN InfoArchive *infoArchiveRepoIdx(unsigned int repoIdx);

// Save archive info for a repo, loading it first when it is not cached
FN_EXTERN void infoArchiveRepoIdxSave(unsigned int repoIdx);

// Backup info for a repo
typedef struct InfoBackupRepoIdxParam
{
    VAR_PARAM_HEADER;
    bool reconstruct;                                               // Add/remove backups to match the repo (see
                                                                    // infoBackupReconstruct)
} InfoBackupRepoIdxParam;

#define infoBackupRepoIdxP(repoIdx, ...)                                                                                           \
    infoBackupRepoIdx(repoIdx, (InfoBackupRepoIdxParam){VAR_PARAM_INIT, __VA_ARGS__})

FN_EXTERN InfoBackup *infoBackupRepoIdx(unsigned int repoIdx, InfoBackupRepoIdxParam param);

// Save backup info for a repo, loading it first when it is not cached
FN_EXTERN void infoBackupRepoIdxSave(unsigned int repoIdx);

#endif
