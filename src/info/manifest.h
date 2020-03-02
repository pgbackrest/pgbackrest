/***********************************************************************************************************************************
Backup Manifest Handler

The backup manifest stores a complete list of all files, links, and paths in a backup along with metadata such as checksums, sizes,
timestamps, etc.  A list of databases is also included for selective restore.

The purpose of the manifest is to allow the restore command to confidently reconstruct the PostgreSQL data directory and ensure that
nothing is missing or corrupt.  It is also useful for reporting, e.g. size of backup, backup time, etc.
***********************************************************************************************************************************/
#ifndef INFO_MANIFEST_H
#define INFO_MANIFEST_H

#include "command/backup/common.h"
#include "common/compress/helper.h"
#include "common/crypto/common.h"
#include "common/type/variantList.h"

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
#define MANIFEST_TYPE                                               Manifest
#define MANIFEST_PREFIX                                             manifest

typedef struct Manifest Manifest;

#include "common/crypto/hash.h"
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
    const String *name;                                             // File name (must be first member in struct)
    bool primary:1;                                                 // Should this file be copied from the primary?
    bool checksumPage:1;                                            // Does this file have page checksums?
    bool checksumPageError:1;                                       // Is there an error in the page checksum?
    mode_t mode;                                                    // File mode
    char checksumSha1[HASH_TYPE_SHA1_SIZE_HEX + 1];                 // SHA1 checksum
    const VariantList *checksumPageErrorList;                       // List of page checksum errors if there are any
    const String *user;                                             // User name
    const String *group;                                            // Group name
    const String *reference;                                        // Reference to a prior backup
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
    const Storage *storagePg, unsigned int pgVersion, bool online, bool checksumPage, const StringList *excludeList,
    const VariantList *tablespaceList);

// Load a manifest from IO
Manifest *manifestNewLoad(IoRead *read);

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
void manifestLinkCheck(const Manifest *this);
Manifest *manifestMove(Manifest *this, MemContext *parentNew);
void manifestSave(Manifest *this, IoWrite *write);

// Validate a completed manifest.  Use strict mode only when saving the manifest after a backup.
void manifestValidate(Manifest *this, bool strict);

/***********************************************************************************************************************************
Db functions and getters/setters
***********************************************************************************************************************************/
const ManifestDb *manifestDb(const Manifest *this, unsigned int dbIdx);
const ManifestDb *manifestDbFind(const Manifest *this, const String *name);
const ManifestDb *manifestDbFindDefault(const Manifest *this, const String *name, const ManifestDb *dbDefault);
unsigned int manifestDbTotal(const Manifest *this);

/***********************************************************************************************************************************
File functions and getters/setters
***********************************************************************************************************************************/
const ManifestFile *manifestFile(const Manifest *this, unsigned int fileIdx);
void manifestFileAdd(Manifest *this, const ManifestFile *file);
const ManifestFile *manifestFileFind(const Manifest *this, const String *name);
const ManifestFile *manifestFileFindDefault(const Manifest *this, const String *name, const ManifestFile *fileDefault);
void manifestFileRemove(const Manifest *this, const String *name);
unsigned int manifestFileTotal(const Manifest *this);

// Update a file with new data
void manifestFileUpdate(
    Manifest *this, const String *name, uint64_t size, uint64_t sizeRepo, const char *checksumSha1, const Variant *reference,
    bool checksumPage, bool checksumPageError, const VariantList *checksumPageErrorList);

/***********************************************************************************************************************************
Link functions and getters/setters
***********************************************************************************************************************************/
const ManifestLink *manifestLink(const Manifest *this, unsigned int linkIdx);
const ManifestLink *manifestLinkFind(const Manifest *this, const String *name);
const ManifestLink *manifestLinkFindDefault(const Manifest *this, const String *name, const ManifestLink *linkDefault);
void manifestLinkRemove(const Manifest *this, const String *name);
unsigned int manifestLinkTotal(const Manifest *this);
void manifestLinkUpdate(const Manifest *this, const String *name, const String *path);

/***********************************************************************************************************************************
Path functions and getters/setters
***********************************************************************************************************************************/
const ManifestPath *manifestPath(const Manifest *this, unsigned int pathIdx);
const ManifestPath *manifestPathFind(const Manifest *this, const String *name);
const ManifestPath *manifestPathFindDefault(const Manifest *this, const String *name, const ManifestPath *pathDefault);

// Data directory relative path for any manifest file/link/path/target name
String *manifestPathPg(const String *manifestPath);

unsigned int manifestPathTotal(const Manifest *this);

/***********************************************************************************************************************************
Target functions and getters/setters
***********************************************************************************************************************************/
const ManifestTarget *manifestTarget(const Manifest *this, unsigned int targetIdx);

// Base target, i.e. the target that is the data directory
const ManifestTarget *manifestTargetBase(const Manifest *this);

const ManifestTarget *manifestTargetFind(const Manifest *this, const String *name);

// Absolute path to the target
String *manifestTargetPath(const Manifest *this, const ManifestTarget *target);

void manifestTargetRemove(const Manifest *this, const String *name);
unsigned int manifestTargetTotal(const Manifest *this);
void manifestTargetUpdate(const Manifest *this, const String *name, const String *path, const String *file);

/***********************************************************************************************************************************
Getters/Setters
***********************************************************************************************************************************/
// Get/set the cipher subpassphrase
const String *manifestCipherSubPass(const Manifest *this);
void manifestCipherSubPassSet(Manifest *this, const String *cipherSubPass);

// Get manifest configuration and options
const ManifestData *manifestData(const Manifest *this);

// Set backup label
void manifestBackupLabelSet(Manifest *this, const String *backupLabel);

/***********************************************************************************************************************************
Helper functions
***********************************************************************************************************************************/
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
