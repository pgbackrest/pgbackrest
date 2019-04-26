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
#include "common/type/stringList.h"
#include "info/infoPg.h"
#include "storage/storage.h"

/***********************************************************************************************************************************
Constants
***********************************************************************************************************************************/
#define INFO_BACKUP_FILE                                            "backup.info"

/***********************************************************************************************************************************
Information about an existing backup
***********************************************************************************************************************************/
typedef struct InfoBackupData
{
    unsigned int backrestFormat;
    const String *backrestVersion;
    const String *backupArchiveStart;
    const String *backupArchiveStop;
    uint64_t backupInfoRepoSize;
    uint64_t backupInfoRepoSizeDelta;
    uint64_t backupInfoSize;
    uint64_t backupInfoSizeDelta;
    const String *backupLabel;
    unsigned int backupPgId;
    const String *backupPrior;
    StringList *backupReference;
    uint64_t backupTimestampStart;
    uint64_t backupTimestampStop;
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
Constructor
***********************************************************************************************************************************/
InfoBackup *infoBackupNew(
    const Storage *storage, const String *fileName, bool ignoreMissing, CipherType cipherType, const String *cipherPass);

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
unsigned int infoBackupCheckPg(
    const InfoBackup *this, unsigned int pgVersion, uint64_t pgSystemId, uint32_t pgCatalogVersion, uint32_t pgControlVersion);
// Remove a backup from the current section
void infoBackupDataDelete(const InfoBackup *this, const String *backupDeleteLabel);
void infoBackupSave(
    InfoBackup *this, const Storage *storage, const String *fileName, CipherType cipherType, const String *cipherPass);

/***********************************************************************************************************************************
infoBackupDataLabelList - get a list of current backup labels
***********************************************************************************************************************************/
typedef struct InfoBackupDataLabelListParam
{
    bool reverse;
    const String *filter;
} InfoBackupDataLabelListParam;

#define infoBackupDataLabelListP(this, ...)                                                                                        \
    infoBackupDataLabelList(this, (InfoBackupDataLabelListParam){__VA_ARGS__})
#define infoBackupDataLabelListNP(this)                                                                                            \
    infoBackupDataLabelList(this, (InfoBackupDataLabelListParam){0})

StringList *infoBackupDataLabelList(const InfoBackup *this, InfoBackupDataLabelListParam param);

/***********************************************************************************************************************************
Getters
***********************************************************************************************************************************/
InfoPg *infoBackupPg(const InfoBackup *this);
InfoBackupData infoBackupData(const InfoBackup *this, unsigned int backupDataIdx);
unsigned int infoBackupDataTotal(const InfoBackup *this);

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
void infoBackupFree(InfoBackup *this);

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
