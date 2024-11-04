/***********************************************************************************************************************************
Backup Info Handler
***********************************************************************************************************************************/
#ifndef INFO_INFOBACKUP_H
#define INFO_INFOBACKUP_H

#include "common/type/stringId.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct InfoBackup InfoBackup;

/***********************************************************************************************************************************
Backup type enum
***********************************************************************************************************************************/
typedef enum
{
    backupTypeFull = STRID5("full", 0x632a60),
    backupTypeDiff = STRID5("diff", 0x319240),
    backupTypeIncr = STRID5("incr", 0x90dc90),
} BackupType;

#include "common/type/object.h"
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
    Variant *backupAnnotation;                                      // Backup annotations, if present
    const String *backupArchiveStart;
    const String *backupArchiveStop;
    uint64_t backupInfoRepoSize;
    uint64_t backupInfoRepoSizeDelta;
    const Variant *backupInfoRepoSizeMap;
    const Variant *backupInfoRepoSizeMapDelta;
    uint64_t backupInfoSize;
    uint64_t backupInfoSizeDelta;
    const String *backupLsnStart;
    const String *backupLsnStop;
    unsigned int backupPgId;
    const String *backupPrior;
    StringList *backupReference;
    time_t backupTimestampStart;
    time_t backupTimestampStop;
    BackupType backupType;
    const Variant *backupError;                                     // Were errors detected during the backup?
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
FN_EXTERN InfoBackup *infoBackupNew(
    unsigned int pgVersion, uint64_t pgSystemId, unsigned int pgCatalogVersion, const String *cipherPassSub);

// Create new object and load contents from IoRead
FN_EXTERN InfoBackup *infoBackupNewLoad(IoRead *read);

/***********************************************************************************************************************************
Getters/Setters
***********************************************************************************************************************************/
typedef struct InfoBackupPub
{
    MemContext *memContext;                                         // Mem context
    InfoPg *infoPg;                                                 // Contents of the DB data
    List *backup;                                                   // List of current backups and their associated data
    bool updated;                                                   // Has the info been updated since the last save?
} InfoBackupPub;

// PostgreSQL info
FN_INLINE_ALWAYS InfoPg *
infoBackupPg(const InfoBackup *const this)
{
    return THIS_PUB(InfoBackup)->infoPg;
}

FN_EXTERN InfoBackup *infoBackupPgSet(InfoBackup *this, unsigned int pgVersion, uint64_t pgSystemId, unsigned int pgCatalogVersion);

// Cipher passphrase
FN_INLINE_ALWAYS const String *
infoBackupCipherPass(const InfoBackup *const this)
{
    return infoPgCipherPass(infoBackupPg(this));
}

// Return a structure of the backup data from a specific index
FN_EXTERN InfoBackupData infoBackupData(const InfoBackup *this, unsigned int backupDataIdx);

// Does the specified backup label exist?
FN_INLINE_ALWAYS bool
infoBackupLabelExists(const InfoBackup *const this, const String *const backupLabel)
{
    ASSERT_INLINE(backupLabel != NULL);
    return lstFind(THIS_PUB(InfoBackup)->backup, &backupLabel) != NULL;
}

// Return a pointer to a structure from the current backup data given a label, else NULL
FN_INLINE_ALWAYS InfoBackupData *
infoBackupDataByLabel(const InfoBackup *const this, const String *const backupLabel)
{
    ASSERT_INLINE(infoBackupLabelExists(this, backupLabel));
    return lstFind(THIS_PUB(InfoBackup)->backup, &backupLabel);
}

// Get total current backups
FN_INLINE_ALWAYS unsigned int
infoBackupDataTotal(const InfoBackup *const this)
{
    return lstSize(THIS_PUB(InfoBackup)->backup);
}

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Add backup to the current list
FN_EXTERN void infoBackupDataAdd(InfoBackup *this, const Manifest *manifest);

// Set Annotation in the backup data for a specific backup label
FN_EXTERN void infoBackupDataAnnotationSet(InfoBackup *this, const String *const backupLabel, const KeyValue *annotationKv);

// Delete backup from the current backup list
FN_EXTERN void infoBackupDataDelete(InfoBackup *this, const String *backupDeleteLabel);

// Given a backup label, get the dependency list
FN_EXTERN StringList *infoBackupDataDependentList(const InfoBackup *this, const String *backupLabel);

// Return a list of current backup labels, applying a regex expression if provided
FN_EXTERN StringList *infoBackupDataLabelList(const InfoBackup *this, const String *expression);

// Move to a new parent mem context
FN_INLINE_ALWAYS InfoBackup *
infoBackupMove(InfoBackup *const this, MemContext *const parentNew)
{
    return objMove(this, parentNew);
}

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
FN_INLINE_ALWAYS void
infoBackupFree(InfoBackup *const this)
{
    objFree(this);
}

/***********************************************************************************************************************************
Helper functions
***********************************************************************************************************************************/
// Load backup info
FN_EXTERN InfoBackup *infoBackupLoadFile(
    const Storage *storage, const String *fileName, CipherType cipherType, const String *cipherPass);

// Load backup info and update it by adding valid backups from the repo or removing backups no longer in the repo
FN_EXTERN InfoBackup *infoBackupLoadFileReconstruct(
    const Storage *storage, const String *fileName, CipherType cipherType, const String *cipherPass);

// Save backup info
FN_EXTERN void infoBackupSaveFile(
    InfoBackup *infoBackup, const Storage *storage, const String *fileName, CipherType cipherType, const String *cipherPass);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
FN_EXTERN void infoBackupDataToLog(const InfoBackupData *this, StringStatic *debugLog);

#define FUNCTION_LOG_INFO_BACKUP_TYPE                                                                                              \
    InfoBackup *
#define FUNCTION_LOG_INFO_BACKUP_FORMAT(value, buffer, bufferSize)                                                                 \
    objNameToLog(value, "InfoBackup", buffer, bufferSize)
#define FUNCTION_LOG_INFO_BACKUP_DATA_TYPE                                                                                         \
    InfoBackupData
#define FUNCTION_LOG_INFO_BACKUP_DATA_FORMAT(value, buffer, bufferSize)                                                            \
    FUNCTION_LOG_OBJECT_FORMAT(&value, infoBackupDataToLog, buffer, bufferSize)

#endif
