/***********************************************************************************************************************************
Archive Info Handler Internal
***********************************************************************************************************************************/
#ifndef INFO_INFOARCHIVE_INTERN_H
#define INFO_INFOARCHIVE_INTERN_H

#include "info/infoArchive.h"

/***********************************************************************************************************************************
Helper functions
***********************************************************************************************************************************/
// Save archive info. Only to be called from the info helper (info/helper.c). Commands save archive info with
// infoArchiveRepoIdxSave().
FN_EXTERN void infoArchiveSaveFile(
    InfoArchive *infoArchive, const Storage *storage, const String *fileName, const CipherSpec *cipherSpec);

#endif
