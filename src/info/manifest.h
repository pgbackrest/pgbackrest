/***********************************************************************************************************************************
Backup Manifest Handler

The backup manifest stores a complete list of all files, links, and paths in a backup along with metadata such as checksums, sizes,
timestamps, etc.  A list of databases is also included for selective restore.

The purpose of the manifest is to allow the restore command to confidently reconstruct the PostgreSQL data directory and ensure that
nothing is missing or corrupt.  It is also useful for reporting, e.g. size of backup, backup time, etc.
***********************************************************************************************************************************/
#ifndef INFO_MANIFEST_H
#define INFO_MANIFEST_H

/***********************************************************************************************************************************
Constants
***********************************************************************************************************************************/
#define BACKUP_MANIFEST_FILE                                        "backup.manifest"
    STRING_DECLARE(BACKUP_MANIFEST_FILE_STR);

#define MANIFEST_TARGET_PGDATA                                      "pg_data"
    STRING_DECLARE(MANIFEST_TARGET_PGDATA_STR);
#define MANIFEST_TARGET_PGTBLSPC                                    "pg_tblspc"
    STRING_DECLARE(MANIFEST_TARGET_PGTBLSPC_STR);

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct Manifest Manifest;

#include "command/backup/common.h"
#include "common/compress/helper.h"
#include "common/crypto/common.h"
#include "common/crypto/hash.h"
#include "common/type/variantList.h"
#include "common/type/object.h"
#include "info/info.h"
#include "info/infoBackup.h"
#include "storage/storage.h"

/***********************************************************************************************************************************
Manifest data
***********************************************************************************************************************************/
typedef struct ManifestData
{
    const String *backrestVersion;                                  // pgBackRest version

    const String *backupLabel;                                      // Backup label (unique identifier for the backup)
    const String *backupLabelPrior;                                 // Backup label for backup this diff/incr is based on
    time_t backupTimestampCopyStart;                                // When did the file copy start?
    time_t backupTimestampStart;                                    // When did the backup start?
    time_t backupTimestampStop;                                     // When did the backup stop?
    BackupType backupType;                                          // Type of backup: full, diff, incr

    // ??? Note that these fields are redundant and verbose since storing the start/stop lsn as a uint64 would be sufficient.
    // However, we currently lack the functions to transform these values back and forth so this will do for now.
    const String *archiveStart;                                     // First WAL file in the backup
    const String *archiveStop;                                      // Last WAL file in the backup
    const String *lsnStart;                                         // Start LSN for the backup
    const String *lsnStop;                                          // Stop LSN for the backup

    unsigned int pgId;                                              // PostgreSQL id in backup.info
    unsigned int pgVersion;                                         // PostgreSQL version
    uint64_t pgSystemId;                                            // PostgreSQL system identifier
    unsigned int pgCatalogVersion;                                  // PostgreSQL catalog version

    bool backupOptionArchiveCheck;                                  // Will WAL segments be checked at the end of the backup?
    bool backupOptionArchiveCopy;                                   // Will WAL segments be copied to the backup?
    const Variant *backupOptionStandby;                             // Will the backup be performed from a standby?
    const Variant *backupOptionBufferSize;                          // Buffer size used for file/protocol operations
    const Variant *backupOptionChecksumPage;                        // Will page checksums be verified?
    CompressType backupOptionCompressType;                          // Compression type used for the backup
    const Variant *backupOptionCompressLevel;                       // Level used for compression (if type not none)
    const Variant *backupOptionCompressLevelNetwork;                // Level used for network compression
    const Variant *backupOptionDelta;                               // Will a checksum delta be performed?
    bool backupOptionHardLink;                                      // Will hardlinks be created in the backup?
    bool backupOptionOnline;                                        // Will an online backup be performed?
    const Variant *backupOptionProcessMax;                          // How many processes will be used for backup?
} ManifestData;

/***********************************************************************************************************************************
Db type
***********************************************************************************************************************************/
typedef struct ManifestDb
{
    const String *name;                                             // Db name (must be first member in struct)
    unsigned int id;                                                // Db oid
    unsigned int lastSystemId;                                      // Highest oid used by system objects in this database
} ManifestDb;

/***********************************************************************************************************************************
File type
***********************************************************************************************************************************/
typedef struct ManifestFile
{
    // Sort
    const String *name;                                             // File name (must be first member in struct)
    uint64_t size;                                                  // Original size
    time_t timestamp;                                               // Original timestamp

    // Backup
    bool primary;                                                   // Should this file be copied from the primary?
    bool checksumPage;                                              // Does this file have page checksums?
    char checksumSha1[HASH_TYPE_SHA1_SIZE_HEX + 1];                 // SHA1 checksum
    const String *reference;                                        // Reference to a prior backup

    // Restore
    mode_t mode;                                                    // File mode
    const String *user;                                             // User name
    const String *group;                                            // Group name
    uint64_t sizeRepo;                                              // Size in repo

    // Info
    bool checksumPageError;                                         // Is there an error in the page checksum?
    const VariantList *checksumPageErrorList;                       // List of page checksum errors if there are any
} ManifestFile;

/***********************************************************************************************************************************
Link type
***********************************************************************************************************************************/
typedef struct ManifestLink
{
    const String *name;                                             // Link name (must be first member in struct)
    const String *destination;                                      // Link destination
    const String *user;                                             // User name
    const String *group;                                            // Group name
} ManifestLink;

/***********************************************************************************************************************************
Path type
***********************************************************************************************************************************/
typedef struct ManifestPath
{
    const String *name;                                             // Path name (must be first member in struct)
    mode_t mode;                                                    // Directory mode
    const String *user;                                             // User name
    const String *group;                                            // Group name
} ManifestPath;

/***********************************************************************************************************************************
Target type
***********************************************************************************************************************************/
typedef enum
{
    manifestTargetTypePath,
    manifestTargetTypeLink,
} ManifestTargetType;

typedef struct ManifestTarget
{
    const String *name;                                             // Target name (must be first member in struct)
    ManifestTargetType type;                                        // Target type
    const String *path;                                             // Target path (if path or link)
    const String *file;                                             // Target file (if file link)
    unsigned int tablespaceId;                                      // Oid if this link is a tablespace
    const String *tablespaceName;                                   // Name of the tablespace
} ManifestTarget;

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
// Build a new manifest for a PostgreSQL data directory
Manifest *manifestNewBuild(
    const Storage *storagePg, unsigned int pgVersion, unsigned int pgCatalogVersion, bool online, bool checksumPage,
    const StringList *excludeList, const VariantList *tablespaceList);

// Load a manifest from IO
Manifest *manifestNewLoad(IoRead *read);

/***********************************************************************************************************************************
Getters/Setters
***********************************************************************************************************************************/
typedef struct ManifestPub
{
    MemContext *memContext;                                         // Mem context
    Info *info;                                                     // Base info object
    ManifestData data;                                              // Manifest data and options
    List *dbList;                                                   // List of databases
    List *fileList;                                                 // List of files
    List *linkList;                                                 // List of links
    List *pathList;                                                 // List of paths
    List *targetList;                                               // List of targets
} ManifestPub;

// Get/set the cipher subpassphrase
__attribute__((always_inline)) static inline const String *
manifestCipherSubPass(const Manifest *const this)
{
    return infoCipherPass(THIS_PUB(Manifest)->info);
}

__attribute__((always_inline)) static inline void
manifestCipherSubPassSet(Manifest *const this, const String *const cipherSubPass)
{
    infoCipherPassSet(THIS_PUB(Manifest)->info, cipherSubPass);
}

// Get manifest configuration and options
__attribute__((always_inline)) static inline const ManifestData *
manifestData(const Manifest *const this)
{
    return &(THIS_PUB(Manifest)->data);
}

// Set backup label
void manifestBackupLabelSet(Manifest *this, const String *backupLabel);

/***********************************************************************************************************************************
Build functions
***********************************************************************************************************************************/
// Validate the timestamps in the manifest given a copy start time, i.e. all times should be <= the copy start time
void manifestBuildValidate(Manifest *this, bool delta, time_t copyStart, CompressType compressType);

// Create a diff/incr backup by comparing to a previous backup manifest
void manifestBuildIncr(Manifest *this, const Manifest *prior, BackupType type, const String *archiveStart);

// Set remaining values before the final save
void manifestBuildComplete(
    Manifest *this, time_t timestampStart, const String *lsnStart, const String *archiveStart, time_t timestampStop,
    const String *lsnStop, const String *archiveStop, unsigned int pgId, uint64_t pgSystemId, const VariantList *dbList,
    bool optionArchiveCheck, bool optionArchiveCopy, size_t optionBufferSize, unsigned int optionCompressLevel,
    unsigned int optionCompressLevelNetwork, bool optionHardLink, unsigned int optionProcessMax, bool optionStandby);

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Ensure that symlinks do not point to the same file, directory, or subdirectory of another link
void manifestLinkCheck(const Manifest *this);

// Move to a new parent mem context
__attribute__((always_inline)) static inline Manifest *
manifestMove(Manifest *const this, MemContext *const parentNew)
{
    return objMove(this, parentNew);
}

// Manifest save
void manifestSave(Manifest *this, IoWrite *write);

// Validate a completed manifest.  Use strict mode only when saving the manifest after a backup.
void manifestValidate(Manifest *this, bool strict);

/***********************************************************************************************************************************
Db functions and getters/setters
***********************************************************************************************************************************/
__attribute__((always_inline)) static inline const ManifestDb *
manifestDb(const Manifest *const this, const unsigned int dbIdx)
{
    return lstGet(THIS_PUB(Manifest)->dbList, dbIdx);
}

const ManifestDb *manifestDbFind(const Manifest *this, const String *name);

// If the database requested is not found in the list, return the default passed rather than throw an error
__attribute__((always_inline)) static inline const ManifestDb *
manifestDbFindDefault(const Manifest *const this, const String *const name, const ManifestDb *const dbDefault)
{
    ASSERT_INLINE(name != NULL);
    return lstFindDefault(THIS_PUB(Manifest)->dbList, &name, (void *)dbDefault);
}

__attribute__((always_inline)) static inline unsigned int
manifestDbTotal(const Manifest *const this)
{
    return lstSize(THIS_PUB(Manifest)->dbList);
}

/***********************************************************************************************************************************
File functions and getters/setters
***********************************************************************************************************************************/
typedef struct ManifestFilePack ManifestFilePack;

ManifestFile manifestFileUnpack(const ManifestFilePack *filePack);

__attribute__((always_inline)) static inline const ManifestFilePack *
manifestFilePackGet(const Manifest *const this, const unsigned int fileIdx)
{
    return *(ManifestFilePack **)lstGet(THIS_PUB(Manifest)->fileList, fileIdx);
}

__attribute__((always_inline)) static inline ManifestFile
manifestFile(const Manifest *const this, const unsigned int fileIdx)
{
    return manifestFileUnpack(manifestFilePackGet(this, fileIdx));
}

void manifestFileAdd(Manifest *this, const ManifestFile *file);
const ManifestFilePack *manifestFilePackFind(const Manifest *this, const String *name);

__attribute__((always_inline)) static inline ManifestFile
manifestFileFind(const Manifest *const this, const String *const name)
{
    ASSERT_INLINE(name != NULL);
    return manifestFileUnpack(manifestFilePackFind(this, name));
}

// If the file requested is not found in the list, return the default passed rather than throw an error
__attribute__((always_inline)) static inline bool
manifestFileExists(const Manifest *const this, const String *const name)
{
    ASSERT_INLINE(name != NULL);
    const String *const *const namePtr = &name;
    return lstFindDefault(THIS_PUB(Manifest)->fileList, &namePtr, NULL) != NULL;
}

void manifestFileRemove(const Manifest *this, const String *name);

__attribute__((always_inline)) static inline unsigned int
manifestFileTotal(const Manifest *const this)
{
    return lstSize(THIS_PUB(Manifest)->fileList);
}

// Update a file with new data
void manifestFileUpdate(
    Manifest *this, const String *name, uint64_t size, uint64_t sizeRepo, const char *checksumSha1, const Variant *reference,
    bool checksumPage, bool checksumPageError, const VariantList *checksumPageErrorList);

/***********************************************************************************************************************************
Link functions and getters/setters
***********************************************************************************************************************************/
__attribute__((always_inline)) static inline const ManifestLink *
manifestLink(const Manifest *const this, const unsigned int linkIdx)
{
    return lstGet(THIS_PUB(Manifest)->linkList, linkIdx);
}

void manifestLinkAdd(Manifest *this, const ManifestLink *link);
const ManifestLink *manifestLinkFind(const Manifest *this, const String *name);

// If the link requested is not found in the list, return the default passed rather than throw an error
__attribute__((always_inline)) static inline const ManifestLink *
manifestLinkFindDefault(const Manifest *const this, const String *const name, const ManifestLink *const linkDefault)
{
    ASSERT_INLINE(name != NULL);
    return lstFindDefault(THIS_PUB(Manifest)->linkList, &name, (void *)linkDefault);
}

void manifestLinkRemove(const Manifest *this, const String *name);

__attribute__((always_inline)) static inline unsigned int
manifestLinkTotal(const Manifest *const this)
{
    return lstSize(THIS_PUB(Manifest)->linkList);
}

void manifestLinkUpdate(const Manifest *this, const String *name, const String *path);

/***********************************************************************************************************************************
Path functions and getters/setters
***********************************************************************************************************************************/
__attribute__((always_inline)) static inline const ManifestPath *
manifestPath(const Manifest *const this, const unsigned int pathIdx)
{
    return lstGet(THIS_PUB(Manifest)->pathList, pathIdx);
}

const ManifestPath *manifestPathFind(const Manifest *this, const String *name);

// If the path requested is not found in the list, return the default passed rather than throw an error
__attribute__((always_inline)) static inline const ManifestPath *
manifestPathFindDefault(const Manifest *const this, const String *const name, const ManifestPath *const pathDefault)
{
    ASSERT_INLINE(name != NULL);
    return lstFindDefault(THIS_PUB(Manifest)->pathList, &name, (void *)pathDefault);
}

// Data directory relative path for any manifest file/link/path/target name
String *manifestPathPg(const String *manifestPath);

__attribute__((always_inline)) static inline unsigned int
manifestPathTotal(const Manifest *const this)
{
    return lstSize(THIS_PUB(Manifest)->pathList);
}

/***********************************************************************************************************************************
Target functions and getters/setters
***********************************************************************************************************************************/
__attribute__((always_inline)) static inline const ManifestTarget *
manifestTarget(const Manifest *const this, const unsigned int targetIdx)
{
    return lstGet(THIS_PUB(Manifest)->targetList, targetIdx);
}

void manifestTargetAdd(Manifest *this, const ManifestTarget *target);
const ManifestTarget *manifestTargetFind(const Manifest *this, const String *name);

// If the target requested is not found in the list, return the default passed rather than throw an error
__attribute__((always_inline)) static inline const ManifestTarget *
manifestTargetFindDefault(const Manifest *const this, const String *const name, const ManifestTarget *const targetDefault)
{
    ASSERT_INLINE(name != NULL);
    return lstFindDefault(THIS_PUB(Manifest)->targetList, &name, (void *)targetDefault);
}

// Base target, i.e. the target that is the data directory
__attribute__((always_inline)) static inline const ManifestTarget *
manifestTargetBase(const Manifest *const this)
{
    return manifestTargetFind(this, MANIFEST_TARGET_PGDATA_STR);
}

// Absolute path to the target
String *manifestTargetPath(const Manifest *this, const ManifestTarget *target);

void manifestTargetRemove(const Manifest *this, const String *name);

__attribute__((always_inline)) static inline unsigned int
manifestTargetTotal(const Manifest *const this)
{
    return lstSize(THIS_PUB(Manifest)->targetList);
}

void manifestTargetUpdate(const Manifest *this, const String *name, const String *path, const String *file);

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
__attribute__((always_inline)) static inline void
manifestFree(Manifest *const this)
{
    objFree(this);
}

/***********************************************************************************************************************************
Helper functions
***********************************************************************************************************************************/
// Load backup manifest
Manifest *manifestLoadFile(const Storage *storage, const String *fileName, CipherType cipherType, const String *cipherPass);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_MANIFEST_TYPE                                                                                                 \
    Manifest *
#define FUNCTION_LOG_MANIFEST_FORMAT(value, buffer, bufferSize)                                                                    \
    objToLog(value, "Manifest", buffer, bufferSize)

#define FUNCTION_LOG_MANIFEST_DB_TYPE                                                                                              \
    ManifestDb *
#define FUNCTION_LOG_MANIFEST_DB_FORMAT(value, buffer, bufferSize)                                                                 \
    objToLog(value, "ManifestDb", buffer, bufferSize)

#define FUNCTION_LOG_MANIFEST_FILE_TYPE                                                                                            \
    ManifestFile *
#define FUNCTION_LOG_MANIFEST_FILE_FORMAT(value, buffer, bufferSize)                                                               \
    objToLog(value, "ManifestFile", buffer, bufferSize)

#define FUNCTION_LOG_MANIFEST_LINK_TYPE                                                                                            \
    ManifestLink *
#define FUNCTION_LOG_MANIFEST_LINK_FORMAT(value, buffer, bufferSize)                                                               \
    objToLog(value, "ManifestLink", buffer, bufferSize)

#define FUNCTION_LOG_MANIFEST_PATH_TYPE                                                                                            \
    ManifestPath *
#define FUNCTION_LOG_MANIFEST_PATH_FORMAT(value, buffer, bufferSize)                                                               \
    objToLog(value, "ManifestPath", buffer, bufferSize)

#define FUNCTION_LOG_MANIFEST_TARGET_TYPE                                                                                          \
    ManifestTarget *
#define FUNCTION_LOG_MANIFEST_TARGET_FORMAT(value, buffer, bufferSize)                                                             \
    objToLog(value, "ManifestTarget", buffer, bufferSize)

#endif
