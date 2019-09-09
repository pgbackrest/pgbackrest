/***********************************************************************************************************************************
Manifest Handler
***********************************************************************************************************************************/
#ifndef INFO_INFOMANIFEST_H
#define INFO_INFOMANIFEST_H

#include "command/backup/common.h"
#include "common/crypto/common.h"
#include "common/type/variantList.h"

/***********************************************************************************************************************************
Constants
***********************************************************************************************************************************/
#define MANIFEST_FILE                                               "backup.manifest"
    STRING_DECLARE(MANIFEST_FILE_STR);

#define MANIFEST_TARGET_PGDATA                                      "pg_data"
    STRING_DECLARE(MANIFEST_TARGET_PGDATA_STR);
#define MANIFEST_TARGET_PGTBLSPC                                    "pg_tblspc"
    STRING_DECLARE(MANIFEST_TARGET_PGTBLSPC_STR);

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct Manifest Manifest;

#include "common/crypto/hash.h"
#include "storage/storage.h"

/***********************************************************************************************************************************
Manifest data
***********************************************************************************************************************************/
typedef struct ManifestData
{
    const String *backupLabel;                                      // Backup label (unique identifier for the backup)
    const String *backupLabelPrior;                                 // Backup label for backup this diff/incr is based on
    time_t backupTimestampCopyStart;                                // When did the file copy start?
    time_t backupTimestampStart;                                    // When did the backup start?
    time_t backupTimestampStop;                                     // When did the backup stop?
    BackupType backupType;                                          // Type of backup: full, diff, incr

    // ??? Note that these fields are redundant and verbose since toring the start/stop lsn as a uint64 would be sufficient.
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
    bool backupOptionCompress;                                      // Will compression be used for backup?
    const Variant *backupOptionCompressLevel;                       // Level to use for compression
    const Variant *backupOptionCompressLevelNetwork;                // Level to use for network compression
    const Variant *backupOptionDelta;                               // Will a checksum delta be performed?
    bool backupOptionHardLink;                                      // Will hardlinks be created in the backup?
    bool backupOptionOnline;                                        // Will an online backup be performed?
    const Variant *backupOptionProcessMax;                          // How many processes will be used for backup?
} ManifestData;

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
Db type
***********************************************************************************************************************************/
typedef struct ManifestDb
{
    const String *name;                                             // Db name (must be first member in struct)
    unsigned int id;                                                // Db oid
    unsigned int lastSystemId;                                      // Highest oid used by system objects in this database
} ManifestDb;

/***********************************************************************************************************************************
Constructor
***********************************************************************************************************************************/
Manifest *manifestNewLoad(IoRead *read);

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
void manifestSave(Manifest *this, IoWrite *write);

/***********************************************************************************************************************************
File functions and getters/setters
***********************************************************************************************************************************/
const ManifestFile *manifestFile(const Manifest *this, unsigned int fileIdx);
const ManifestFile *manifestFileFind(const Manifest *this, const String *name);
unsigned int manifestFileTotal(const Manifest *this);

/***********************************************************************************************************************************
Link functions and getters/setters
***********************************************************************************************************************************/
const ManifestLink *manifestLink(const Manifest *this, unsigned int linkIdx);
const ManifestLink *manifestLinkFind(const Manifest *this, const String *name);
// const ManifestLink *manifestLinkFindDefault(const Manifest *this, const String *name, const ManifestLink *linkDefault);
void manifestLinkRemove(const Manifest *this, const String *name);
unsigned int manifestLinkTotal(const Manifest *this);
void manifestLinkUpdate(const Manifest *this, const String *name, const String *path);

/***********************************************************************************************************************************
Path functions and getters/setters
***********************************************************************************************************************************/
const ManifestPath *manifestPath(const Manifest *this, unsigned int pathIdx);
const ManifestPath *manifestPathFind(const Manifest *this, const String *name);
unsigned int manifestPathTotal(const Manifest *this);

/***********************************************************************************************************************************
Target functions and getters/setters
***********************************************************************************************************************************/
const ManifestTarget *manifestTarget(const Manifest *this, unsigned int targetIdx);
const ManifestTarget *manifestTargetFind(const Manifest *this, const String *name);
// const ManifestTarget *manifestTargetFindDefault(const Manifest *this, const String *name, const ManifestTarget *targetDefault);
void manifestTargetRemove(const Manifest *this, const String *name);
unsigned int manifestTargetTotal(const Manifest *this);
void manifestTargetUpdate(const Manifest *this, const String *name, const String *path, const String *file);

/***********************************************************************************************************************************
Getters
***********************************************************************************************************************************/
const ManifestData *manifestData(const Manifest *this);
String *manifestPgPath(const String *manifestPath);

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

#define FUNCTION_LOG_MANIFEST_FILE_TYPE                                                                                            \
    ManifestFile *
#define FUNCTION_LOG_MANIFEST_FILE_FORMAT(value, buffer, bufferSize)                                                               \
    objToLog(value, "ManifestFile", buffer, bufferSize)

#define FUNCTION_LOG_MANIFEST_PATH_TYPE                                                                                            \
    ManifestPath *
#define FUNCTION_LOG_MANIFEST_PATH_FORMAT(value, buffer, bufferSize)                                                               \
    objToLog(value, "ManifestPath", buffer, bufferSize)

#define FUNCTION_LOG_MANIFEST_TARGET_TYPE                                                                                          \
    ManifestTarget *
#define FUNCTION_LOG_MANIFEST_TARGET_FORMAT(value, buffer, bufferSize)                                                             \
    objToLog(value, "ManifestTarget", buffer, bufferSize)

#endif
