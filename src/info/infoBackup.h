/***********************************************************************************************************************************
Backup Info Handler
***********************************************************************************************************************************/
#ifndef INFO_INFOBACKUP_H
#define INFO_INFOBACKUP_H

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct InfoBackup InfoBackup;

#include "common/type/string.h"
#include "info/infoPg.h"
#include "storage/storage.h"

/***********************************************************************************************************************************
Constants
***********************************************************************************************************************************/
#define INFO_BACKUP_FILE                                           "backup.info"

STRING_DECLARE(INFO_BACKUP_KEY_BACKUP_INFO_REPO_SIZE_STR);
STRING_DECLARE(INFO_BACKUP_KEY_BACKUP_INFO_REPO_SIZE_DELTA_STR);
STRING_DECLARE(INFO_BACKUP_KEY_BACKUP_INFO_SIZE_STR);
STRING_DECLARE(INFO_BACKUP_KEY_BACKUP_INFO_SIZE_DELTA_STR);
STRING_DECLARE(INFO_BACKUP_KEY_BACKUP_REFERENCE_STR);

/***********************************************************************************************************************************
Constructor
***********************************************************************************************************************************/
InfoBackup *infoBackupNew(const Storage *storage, const String *fileName, bool ignoreMissing);

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
unsigned int infoBackupCheckPg(
    const InfoBackup *this, unsigned int pgVersion, uint64_t pgSystemId, uint32_t pgCatalogVersion, uint32_t pgControlVersion);
StringList *infoBackupCurrentKeyGet(const InfoBackup *this);

/***********************************************************************************************************************************
Getters
***********************************************************************************************************************************/
InfoPg *infoBackupPg(const InfoBackup *this);
const Variant *infoBackupCurrentGet(const InfoBackup *this, const String *section, const String *key);

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
void infoBackupFree(InfoBackup *this);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_DEBUG_INFO_BACKUP_TYPE                                                                                           \
    InfoBackup *
#define FUNCTION_DEBUG_INFO_BACKUP_FORMAT(value, buffer, bufferSize)                                                              \
    objToLog(value, "InfoBackup", buffer, bufferSize)

#endif
