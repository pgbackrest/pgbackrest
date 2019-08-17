/***********************************************************************************************************************************
Manifest Info Handler
***********************************************************************************************************************************/
#ifndef INFO_INFOMANIFEST_H
#define INFO_INFOMANIFEST_H

#include "command/backup/common.h"
#include "common/crypto/common.h"
#include "common/type/variantList.h"

/***********************************************************************************************************************************
Constants
***********************************************************************************************************************************/
#define INFO_MANIFEST_FILE                                          "backup.manifest"

#define INFO_MANIFEST_KEY_BACKUP_ARCHIVE_START                      "backup-archive-start"
    VARIANT_DECLARE(INFO_MANIFEST_KEY_BACKUP_ARCHIVE_START_VAR);
#define INFO_MANIFEST_KEY_BACKUP_ARCHIVE_STOP                       "backup-archive-stop"
    VARIANT_DECLARE(INFO_MANIFEST_KEY_BACKUP_ARCHIVE_STOP_VAR);
#define INFO_MANIFEST_KEY_BACKUP_PRIOR                              "backup-prior"
    VARIANT_DECLARE(INFO_MANIFEST_KEY_BACKUP_PRIOR_VAR);
#define INFO_MANIFEST_KEY_BACKUP_TIMESTAMP_START                    "backup-timestamp-start"
    VARIANT_DECLARE(INFO_MANIFEST_KEY_BACKUP_TIMESTAMP_START_VAR);
#define INFO_MANIFEST_KEY_BACKUP_TIMESTAMP_STOP                     "backup-timestamp-stop"
    VARIANT_DECLARE(INFO_MANIFEST_KEY_BACKUP_TIMESTAMP_STOP_VAR);
#define INFO_MANIFEST_KEY_BACKUP_TYPE                               "backup-type"
    VARIANT_DECLARE(INFO_MANIFEST_KEY_BACKUP_TYPE_VAR);
#define INFO_MANIFEST_KEY_OPT_ARCHIVE_CHECK                         "option-archive-check"
    VARIANT_DECLARE(INFO_MANIFEST_KEY_OPT_ARCHIVE_CHECK_VAR);
#define INFO_MANIFEST_KEY_OPT_ARCHIVE_COPY                          "option-archive-copy"
    VARIANT_DECLARE(INFO_MANIFEST_KEY_OPT_ARCHIVE_COPY_VAR);
#define INFO_MANIFEST_KEY_OPT_BACKUP_STANDBY                        "option-backup-standby"
    VARIANT_DECLARE(INFO_MANIFEST_KEY_OPT_BACKUP_STANDBY_VAR);
#define INFO_MANIFEST_KEY_OPT_CHECKSUM_PAGE                         "option-checksum-page"
    VARIANT_DECLARE(INFO_MANIFEST_KEY_OPT_CHECKSUM_PAGE_VAR);
#define INFO_MANIFEST_KEY_OPT_COMPRESS                              "option-compress"
    VARIANT_DECLARE(INFO_MANIFEST_KEY_OPT_COMPRESS_VAR);
#define INFO_MANIFEST_KEY_OPT_HARDLINK                              "option-hardlink"
    VARIANT_DECLARE(INFO_MANIFEST_KEY_OPT_HARDLINK_VAR);
#define INFO_MANIFEST_KEY_OPT_ONLINE                                "option-online"
    VARIANT_DECLARE(INFO_MANIFEST_KEY_OPT_ONLINE_VAR);

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct InfoManifest InfoManifest;

#include "common/crypto/hash.h"
#include "storage/storage.h"

/***********************************************************************************************************************************
Manifest data
***********************************************************************************************************************************/
typedef struct InfoManifestData
{
    const String *backupLabel;                                      // Backup label (unique identifier for the backup)
    time_t backupTimestampCopyStart;                                // When did the file copy start?
    time_t backupTimestampStart;                                    // When did the backup start?
    time_t backupTimestampStop;                                     // When did the backup stop?
    BackupType backupType;                                          // Type of backup: full, diff, incr

    unsigned int pgId;                                              // PostgreSQL id in backup.info
    unsigned int pgVersion;                                         // PostgreSQL version
    uint64_t pgSystemId;                                            // PostgreSQL system identifier
    uint32_t pgControlVersion;                                      // PostgreSQL control version
    uint32_t pgCatalogVersion;                                      // PostgreSQL catalog version
} InfoManifestData;

/***********************************************************************************************************************************
Target type
***********************************************************************************************************************************/
typedef enum
{
    manifestTargetTypePath,
    manifestTargetTypeLink,
} ManifestTargetType;

typedef struct InfoManifestTarget
{
    const String *name;                                             // Target name
    ManifestTargetType type;                                        // Target type
    const String *path;                                             // Target path (if path or link)
    const String *file;                                             // Target file (if file link)
} InfoManifestTarget;

/***********************************************************************************************************************************
Path type
***********************************************************************************************************************************/
typedef struct InfoManifestPath
{
    const String *name;                                             // Path name
    bool base:1;                                                    // Is this the base path?
    bool db:1;                                                      // Does this path contain db relation files?
    mode_t mode;                                                    // Directory mode
    const String *user;                                             // User name
    const String *group;                                            // Group name
} InfoManifestPath;

/***********************************************************************************************************************************
File type
***********************************************************************************************************************************/
typedef struct InfoManifestFile
{
    const String *name;                                             // File name
    bool master:1;                                                  // Should this file be copied from master?
    bool checksumPage:1;                                            // Does this file have page checksums?
    mode_t mode;                                                    // File mode
    char checksumSha1[HASH_TYPE_SHA1_SIZE_HEX + 1];                 // SHA1 checksum
    const VariantList *checksumPageError;                           // List if page checksum errors if there are any
    const String *user;                                             // User name
    const String *group;                                            // Group name
    uint64_t size;                                                  // Original size
    uint64_t sizeRepo;                                              // Size in repo
    time_t timestamp;                                               // Original timestamp
} InfoManifestFile;

/***********************************************************************************************************************************
Link type
***********************************************************************************************************************************/
typedef struct InfoManifestLink
{
    const String *name;                                             // Link name
    const String *destination;                                      // Link destination
    const String *user;                                             // User name
    const String *group;                                            // Group name
} InfoManifestLink;

/***********************************************************************************************************************************
Constructor
***********************************************************************************************************************************/
InfoManifest *infoManifestNewLoad(
    const Storage *storagePg, const String *fileName, CipherType cipherType, const String *cipherPass);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_INFO_MANIFEST_TYPE                                                                                            \
    InfoManifest *
#define FUNCTION_LOG_INFO_MANIFEST_FORMAT(value, buffer, bufferSize)                                                               \
    objToLog(value, "InfoManifest", buffer, bufferSize)

#endif
