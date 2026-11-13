/***********************************************************************************************************************************
Backup Info Handler Internal
***********************************************************************************************************************************/
#ifndef INFO_INFOBACKUP_INTERN_H
#define INFO_INFOBACKUP_INTERN_H

#include "info/infoBackup.h"

/***********************************************************************************************************************************
Helper functions
***********************************************************************************************************************************/
// Save backup info. Only to be called from the info helper (info/helper.c). Commands save backup info with infoBackupRepoIdxSave().
// The file is encrypted with the prior cipher spec when it was decrypted with the prior cipher spec.
FN_EXTERN void infoBackupSaveFile(
    InfoBackup *infoBackup, const Storage *storage, const String *fileName, const CipherSpec *cipherSpec);

#endif
