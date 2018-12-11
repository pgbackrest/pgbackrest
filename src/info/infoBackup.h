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
#define INFO_BACKUP_FILE                                            "backup.info"

#define INFO_BACKUP_KEY_BACKUP_INFO_REPO_SIZE                       "backup-info-repo-size"
    STRING_DECLARE(INFO_BACKUP_KEY_BACKUP_INFO_REPO_SIZE_STR);
#define INFO_BACKUP_KEY_BACKUP_INFO_REPO_SIZE_DELTA                 "backup-info-repo-size-delta"
    STRING_DECLARE(INFO_BACKUP_KEY_BACKUP_INFO_REPO_SIZE_DELTA_STR);
#define INFO_BACKUP_KEY_BACKUP_INFO_SIZE                            "backup-info-size"
    STRING_DECLARE(INFO_BACKUP_KEY_BACKUP_INFO_SIZE_STR);
#define INFO_BACKUP_KEY_BACKUP_INFO_SIZE_DELTA                      "backup-info-size-delta"
    STRING_DECLARE(INFO_BACKUP_KEY_BACKUP_INFO_SIZE_DELTA_STR);
#define INFO_BACKUP_KEY_BACKUP_REFERENCE                            "backup-reference"
    STRING_DECLARE(INFO_BACKUP_KEY_BACKUP_REFERENCE_STR);

/***********************************************************************************************************************************
Information about an existing backup
***********************************************************************************************************************************/
typedef struct InfoBackupData
{
    const String *backupLabel;
    int backrestFormat;
    const String *backrestVersion;
    const String *backupArchiveStart;
    const String *backupArchiveStop;
    uint64_t backupInfoRepoSize;
    uint64_t backupInfoRepoSizeDelta;
    uint64_t backupInfoSize;
    uint64_t backupInfoSizeDelta;
    const String *backupPrior;
    StringList *backupReference;
    const String *backupType;
    unsigned int backupPgId;
    // bool optionArchiveCheck;
    // bool optionArchiveCopy;
    // bool optionBackupStandby;
    // bool optionChecksumPage;
    // bool optionCompress;
    // bool optionHardlink;
    // bool optionOnline;
} InfoBackupData;

/***********************************************************************************************************************************
Constructor
***********************************************************************************************************************************/
InfoBackup *infoBackupNew(
    const Storage *storage, const String *fileName, bool ignoreMissing, CipherType cipherType, const String *cipherPass);

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
InfoBackupData *infoBackupDataList(const InfoBackup *this);
InfoBackupData infoBackupData(const InfoBackup *this, unsigned int backupDataIdx);

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
#define FUNCTION_DEBUG_INFO_BACKUP_DATA_TYPE                                                                                           \
    InfoBackupData
#define FUNCTION_DEBUG_INFO_BACKUP_DATA_FORMAT(value, buffer, bufferSize)                                                              \
    FUNCTION_DEBUG_STRING_OBJECT_FORMAT(&value, infoBackupDataToLog, buffer, bufferSize)
#define FUNCTION_DEBUG_INFO_BACKUP_DATAP_TYPE                                                                                          \
    InfoBackupData *
#define FUNCTION_DEBUG_INFO_BACKUP_DATAP_FORMAT(value, buffer, bufferSize)                                                             \
    FUNCTION_DEBUG_STRING_OBJECT_FORMAT(value, infoBackupDataToLog, buffer, bufferSize)

#endif
