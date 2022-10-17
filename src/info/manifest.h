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
#define BACKUP_MANIFEST_EXT                                         ".manifest"
#define BACKUP_MANIFEST_FILE                                        "backup" BACKUP_MANIFEST_EXT
    STRING_DECLARE(BACKUP_MANIFEST_FILE_STR);

#define MANIFEST_PATH_BUNDLE                                        "bundle"
    STRING_DECLARE(MANIFEST_PATH_BUNDLE_STR);

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
#include "common/type/variant.h"
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
    bool bundle;                                                    // Does the backup bundle files?

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

    const Variant *annotation;                                      // Backup annotation(s) metadata

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
    unsigned int lastSystemId;                                      // Highest oid used by system objects (deprecated - do not use)
} ManifestDb;

/***********************************************************************************************************************************
File type
***********************************************************************************************************************************/
typedef struct ManifestFile
{
    const String *name;                                             // File name (must be first member in struct)
    bool copy:1;                                                    // Should the file be copied (backup only)?
    bool delta:1;                                                   // Verify checksum in PGDATA before copying (backup only)?
    bool resume:1;                                                  // Is the file being resumed (backup only)?
    bool checksumPage:1;                                            // Does this file have page checksums?
    bool checksumPageError:1;                                       // Is there an error in the page checksum?
    mode_t mode;                                                    // File mode
    char checksumSha1[HASH_TYPE_SHA1_SIZE_HEX + 1];                 // SHA1 checksum
    const String *checksumPageErrorList;                            // List of page checksum errors if there are any
    const String *user;                                             // User name
    const String *group;                                            // Group name
    const String *reference;                                        // Reference to a prior backup
    uint64_t bundleId;                                              // Bundle id
    uint64_t bundleOffset;                                          // Bundle offset
    uint64_t size;                                                  // Original size
    uint64_t sizeRepo;                                              // Size in repo
    time_t timestamp;                                               // Original timestamp
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
    const Storage *storagePg, unsigned int pgVersion, unsigned int pgCatalogVersion, bool online, bool checksumPage, bool bundle,
    const StringList *excludeList, const Pack *tablespaceList);

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
    StringList *referenceList;                                      // List of file references
} ManifestPub;

// Get/set the cipher subpassphrase
FN_INLINE_ALWAYS const String *
manifestCipherSubPass(const Manifest *const this)
{
    return infoCipherPass(THIS_PUB(Manifest)->info);
}

FN_INLINE_ALWAYS void
manifestCipherSubPassSet(Manifest *const this, const String *const cipherSubPass)
{
    infoCipherPassSet(THIS_PUB(Manifest)->info, cipherSubPass);
}

// Get manifest configuration and options
FN_INLINE_ALWAYS const ManifestData *
manifestData(const Manifest *const this)
{
    return &(THIS_PUB(Manifest)->data);
}

// Get reference list
FN_INLINE_ALWAYS const StringList *
manifestReferenceList(const Manifest *const this)
{
    return THIS_PUB(Manifest)->referenceList;
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
    const String *lsnStop, const String *archiveStop, unsigned int pgId, uint64_t pgSystemId, const Pack *dbList,
    bool optionArchiveCheck, bool optionArchiveCopy, size_t optionBufferSize, unsigned int optionCompressLevel,
    unsigned int optionCompressLevelNetwork, bool optionHardLink, unsigned int optionProcessMax, bool optionStandby,
    const KeyValue *annotation);

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Ensure that symlinks do not point to the same file, directory, or subdirectory of another link
void manifestLinkCheck(const Manifest *this);

// Move to a new parent mem context
FN_INLINE_ALWAYS Manifest *
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
FN_INLINE_ALWAYS const ManifestDb *
manifestDb(const Manifest *const this, const unsigned int dbIdx)
{
    return lstGet(THIS_PUB(Manifest)->dbList, dbIdx);
}

const ManifestDb *manifestDbFind(const Manifest *this, const String *name);

// If the database requested is not found in the list, return the default passed rather than throw an error
FN_INLINE_ALWAYS const ManifestDb *
manifestDbFindDefault(const Manifest *const this, const String *const name, const ManifestDb *const dbDefault)
{
    ASSERT_INLINE(name != NULL);
    return lstFindDefault(THIS_PUB(Manifest)->dbList, &name, (void *)dbDefault);
}

FN_INLINE_ALWAYS unsigned int
manifestDbTotal(const Manifest *const this)
{
    return lstSize(THIS_PUB(Manifest)->dbList);
}

/***********************************************************************************************************************************
File functions and getters/setters
***********************************************************************************************************************************/
typedef struct ManifestFilePack ManifestFilePack;

// Unpack file pack returned by manifestFilePackGet()
ManifestFile manifestFileUnpack(const Manifest *manifest, const ManifestFilePack *filePack);

// Get file in pack format by index
FN_INLINE_ALWAYS const ManifestFilePack *
manifestFilePackGet(const Manifest *const this, const unsigned int fileIdx)
{
    return *(ManifestFilePack **)lstGet(THIS_PUB(Manifest)->fileList, fileIdx);
}

// Get file name
FN_INLINE_ALWAYS const String *
manifestFileNameGet(const Manifest *const this, const unsigned int fileIdx)
{
    return (const String *)manifestFilePackGet(this, fileIdx);
}

// Get file by index
FN_INLINE_ALWAYS ManifestFile
manifestFile(const Manifest *const this, const unsigned int fileIdx)
{
    return manifestFileUnpack(this, manifestFilePackGet(this, fileIdx));
}

// Add a file
void manifestFileAdd(Manifest *this, ManifestFile *file);

// Find file in pack format by name
const ManifestFilePack *manifestFilePackFind(const Manifest *this, const String *name);

// Find file by name
FN_INLINE_ALWAYS ManifestFile
manifestFileFind(const Manifest *const this, const String *const name)
{
    ASSERT_INLINE(name != NULL);
    return manifestFileUnpack(this, manifestFilePackFind(this, name));
}

// Does the file exist?
FN_INLINE_ALWAYS bool
manifestFileExists(const Manifest *const this, const String *const name)
{
    ASSERT_INLINE(name != NULL);
    return lstFindDefault(THIS_PUB(Manifest)->fileList, &name, NULL) != NULL;
}

void manifestFileRemove(const Manifest *this, const String *name);

FN_INLINE_ALWAYS unsigned int
manifestFileTotal(const Manifest *const this)
{
    return lstSize(THIS_PUB(Manifest)->fileList);
}

// Update a file with new data
void manifestFileUpdate(Manifest *const this, const ManifestFile *file);

/***********************************************************************************************************************************
Link functions and getters/setters
***********************************************************************************************************************************/
FN_INLINE_ALWAYS const ManifestLink *
manifestLink(const Manifest *const this, const unsigned int linkIdx)
{
    return lstGet(THIS_PUB(Manifest)->linkList, linkIdx);
}

void manifestLinkAdd(Manifest *this, const ManifestLink *link);
const ManifestLink *manifestLinkFind(const Manifest *this, const String *name);

// If the link requested is not found in the list, return the default passed rather than throw an error
FN_INLINE_ALWAYS const ManifestLink *
manifestLinkFindDefault(const Manifest *const this, const String *const name, const ManifestLink *const linkDefault)
{
    ASSERT_INLINE(name != NULL);
    return lstFindDefault(THIS_PUB(Manifest)->linkList, &name, (void *)linkDefault);
}

void manifestLinkRemove(const Manifest *this, const String *name);

FN_INLINE_ALWAYS unsigned int
manifestLinkTotal(const Manifest *const this)
{
    return lstSize(THIS_PUB(Manifest)->linkList);
}

void manifestLinkUpdate(const Manifest *this, const String *name, const String *path);

/***********************************************************************************************************************************
Path functions and getters/setters
***********************************************************************************************************************************/
FN_INLINE_ALWAYS const ManifestPath *
manifestPath(const Manifest *const this, const unsigned int pathIdx)
{
    return lstGet(THIS_PUB(Manifest)->pathList, pathIdx);
}

const ManifestPath *manifestPathFind(const Manifest *this, const String *name);

// If the path requested is not found in the list, return the default passed rather than throw an error
FN_INLINE_ALWAYS const ManifestPath *
manifestPathFindDefault(const Manifest *const this, const String *const name, const ManifestPath *const pathDefault)
{
    ASSERT_INLINE(name != NULL);
    return lstFindDefault(THIS_PUB(Manifest)->pathList, &name, (void *)pathDefault);
}

// Data directory relative path for any manifest file/link/path/target name
String *manifestPathPg(const String *manifestPath);

FN_INLINE_ALWAYS unsigned int
manifestPathTotal(const Manifest *const this)
{
    return lstSize(THIS_PUB(Manifest)->pathList);
}

/***********************************************************************************************************************************
Target functions and getters/setters
***********************************************************************************************************************************/
FN_INLINE_ALWAYS const ManifestTarget *
manifestTarget(const Manifest *const this, const unsigned int targetIdx)
{
    return lstGet(THIS_PUB(Manifest)->targetList, targetIdx);
}

void manifestTargetAdd(Manifest *this, const ManifestTarget *target);
const ManifestTarget *manifestTargetFind(const Manifest *this, const String *name);

// If the target requested is not found in the list, return the default passed rather than throw an error
FN_INLINE_ALWAYS const ManifestTarget *
manifestTargetFindDefault(const Manifest *const this, const String *const name, const ManifestTarget *const targetDefault)
{
    ASSERT_INLINE(name != NULL);
    return lstFindDefault(THIS_PUB(Manifest)->targetList, &name, (void *)targetDefault);
}

// Base target, i.e. the target that is the data directory
FN_INLINE_ALWAYS const ManifestTarget *
manifestTargetBase(const Manifest *const this)
{
    return manifestTargetFind(this, MANIFEST_TARGET_PGDATA_STR);
}

// Absolute path to the target
String *manifestTargetPath(const Manifest *this, const ManifestTarget *target);

void manifestTargetRemove(const Manifest *this, const String *name);

FN_INLINE_ALWAYS unsigned int
manifestTargetTotal(const Manifest *const this)
{
    return lstSize(THIS_PUB(Manifest)->targetList);
}

void manifestTargetUpdate(const Manifest *this, const String *name, const String *path, const String *file);

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
FN_INLINE_ALWAYS void
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
