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
    BackupType backupType;
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
InfoBackup *infoBackupNew(unsigned int pgVersion, uint64_t pgSystemId, unsigned int pgCatalogVersion, const String *cipherPassSub);

// Create new object and load contents from IoRead
InfoBackup *infoBackupNewLoad(IoRead *read);

/***********************************************************************************************************************************
Getters/Setters
***********************************************************************************************************************************/
typedef struct InfoBackupPub
{
    MemContext *memContext;                                         // Mem context
    InfoPg *infoPg;                                                 // Contents of the DB data
    List *backup;                                                   // List of current backups and their associated data
} InfoBackupPub;

// PostgreSQL info
__attribute__((always_inline)) static inline InfoPg *
infoBackupPg(const InfoBackup *const this)
{
    return THIS_PUB(InfoBackup)->infoPg;
}

InfoBackup *infoBackupPgSet(InfoBackup *this, unsigned int pgVersion, uint64_t pgSystemId, unsigned int pgCatalogVersion);

// Cipher passphrase
__attribute__((always_inline)) static inline const String *
infoBackupCipherPass(const InfoBackup *const this)
{
    return infoPgCipherPass(infoBackupPg(this));
}

// Return a structure of the backup data from a specific index
InfoBackupData infoBackupData(const InfoBackup *this, unsigned int backupDataIdx);

// Return a pointer to a structure from the current backup data given a label, else NULL
__attribute__((always_inline)) static inline InfoBackupData *
infoBackupDataByLabel(const InfoBackup *const this, const String *const backupLabel)
{
    ASSERT_INLINE(backupLabel != NULL);
    return lstFind(THIS_PUB(InfoBackup)->backup, &backupLabel);
}

// Get total current backups
__attribute__((always_inline)) static inline unsigned int
infoBackupDataTotal(const InfoBackup *const this)
{
    return lstSize(THIS_PUB(InfoBackup)->backup);
}

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Add backup to the current list
void infoBackupDataAdd(const InfoBackup *this, const Manifest *manifest);

// Delete backup from the current backup list
void infoBackupDataDelete(const InfoBackup *this, const String *backupDeleteLabel);

// Given a backup label, get the dependency list
StringList *infoBackupDataDependentList(const InfoBackup *this, const String *backupLabel);

// Return a list of current backup labels, applying a regex expression if provided
StringList *infoBackupDataLabelList(const InfoBackup *this, const String *expression);

// Move to a new parent mem context
__attribute__((always_inline)) static inline InfoBackup *
infoBackupMove(InfoBackup *const this, MemContext *const parentNew)
{
    return objMove(this, parentNew);
}

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
__attribute__((always_inline)) static inline void
infoBackupFree(InfoBackup *const this)
{
    objFree(this);
}

/***********************************************************************************************************************************
Helper functions
***********************************************************************************************************************************/
// Load backup info
InfoBackup *infoBackupLoadFile(
    const Storage *storage, const String *fileName, CipherType cipherType, const String *cipherPass);

// Load backup info and update it by adding valid backups from the repo or removing backups no longer in the repo
InfoBackup *infoBackupLoadFileReconstruct(
    const Storage *storage, const String *fileName, CipherType cipherType, const String *cipherPass);

// Save backup info
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
