/***********************************************************************************************************************************
Backup Info Handler
***********************************************************************************************************************************/
#ifndef INFO_INFOBACKUP_H
#define INFO_INFOBACKUP_H

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
#define INFO_BACKUP_TYPE                                            InfoBackup
#define INFO_BACKUP_PREFIX                                          infoBackup

typedef struct InfoBackup InfoBackup;

#include "common/type/string.h"
#include "common/type/stringList.h"
#include "info/infoPg.h"
#include "info/manifest.h"
#include "storage/storage.h"

/***********************************************************************************************************************************
Constants
***********************************************************************************************************************************/
#define INFO_BACKUP_FILE                                            "backup.info"

#define INFO_BACKUP_PATH_FILE                                       STORAGE_REPO_BACKUP "/" INFO_BACKUP_FILE
    STRING_DECLARE(INFO_BACKUP_PATH_FILE_STR);
#define INFO_BACKUP_PATH_FILE_COPY                                  INFO_BACKUP_PATH_FILE INFO_COPY_EXT
    STRING_DECLARE(INFO_BACKUP_PATH_FILE_COPY_STR);

/***********************************************************************************************************************************
Information about an existing backup
***********************************************************************************************************************************/
typedef struct InfoBackupData
{
    const String *backupLabel;                                      // backupLabel must be first to allow for built-in list sorting
    unsigned int backrestFormat;
    const String *backrestVersion;
    const String *backupArchiveStart;
    const String *backupArchiveStop;
    uint64_t backupInfoRepoSize;
    uint64_t backupInfoRepoSizeDelta;
    uint64_t backupInfoSize;
    uint64_t backupInfoSizeDelta;
    unsigned int backupPgId;
    const String *backupPrior;
    StringList *backupReference;
    time_t backupTimestampStart;
    time_t backupTimestampStop;
    const String *backupType;
    bool optionArchiveCheck;
    bool optionArchiveCopy;
    bool optionBackupStandby;
    bool optionChecksumPage;
    bool optionCompress;
    bool optionHardlink;
    bool optionOnline;
} InfoBackupData;

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
InfoBackup *infoBackupNew(unsigned int pgVersion, uint64_t pgSystemId, const String *cipherPassSub);

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Add backup to current section
void infoBackupDataAdd(const InfoBackup *this, const Manifest *manifest);
// Remove a backup from the current section
void infoBackupDataDelete(const InfoBackup *this, const String *backupDeleteLabel);
InfoBackup *infoBackupPgSet(InfoBackup *this, unsigned int pgVersion, uint64_t pgSystemId);

/***********************************************************************************************************************************
Getters
***********************************************************************************************************************************/
// Get a list of current backup labels
StringList *infoBackupDataLabelList(const InfoBackup *this, const String *expression);

InfoPg *infoBackupPg(const InfoBackup *this);
InfoBackupData infoBackupData(const InfoBackup *this, unsigned int backupDataIdx);

// Return a pointer to a structure from the current backup data given a label, else NULL
InfoBackupData *infoBackupDataByLabel(const InfoBackup *this, const String *backupLabel);

// Given a backup label, get the dependency list
StringList *infoBackupDataDependentList(const InfoBackup *this, const String *backupLabel);

unsigned int infoBackupDataTotal(const InfoBackup *this);
const String *infoBackupCipherPass(const InfoBackup *this);

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
void infoBackupFree(InfoBackup *this);

/***********************************************************************************************************************************
Helper functions
***********************************************************************************************************************************/
InfoBackup *infoBackupLoadFile(
    const Storage *storage, const String *fileName, CipherType cipherType, const String *cipherPass);
InfoBackup *infoBackupLoadFileReconstruct(
    const Storage *storage, const String *fileName, CipherType cipherType, const String *cipherPass);
void infoBackupSaveFile(
    InfoBackup *infoBackup, const Storage *storage, const String *fileName, CipherType cipherType, const String *cipherPass);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
String *infoBackupDataToLog(const InfoBackupData *this);

#define FUNCTION_LOG_INFO_BACKUP_TYPE                                                                                              \
    InfoBackup *
#define FUNCTION_LOG_INFO_BACKUP_FORMAT(value, buffer, bufferSize)                                                                 \
    objToLog(value, "InfoBackup", buffer, bufferSize)
#define FUNCTION_LOG_INFO_BACKUP_DATA_TYPE                                                                                         \
    InfoBackupData
#define FUNCTION_LOG_INFO_BACKUP_DATA_FORMAT(value, buffer, bufferSize)                                                            \
    FUNCTION_LOG_STRING_OBJECT_FORMAT(&value, infoBackupDataToLog, buffer, bufferSize)

#endif
