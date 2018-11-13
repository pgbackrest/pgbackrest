/***********************************************************************************************************************************
Manifest Info Handler
***********************************************************************************************************************************/
#include "common/type/string.h"

/* CSHANG It is not good to have a dependency on a config name so they've been removed. We have renamed config options before and
it's OK to affect the config file but changes to the info files would require a format version bump. option-hardlink is a prime
example as the config option name changed to repo-hardlink
*/
// CSHANG Replaces MANIFEST_KEY_BACKUP_STANDBY and INFO_BACKUP_KEY_BACKUP_STANDBY
STRING_EXTERN(INFO_MANIFEST_KEY_BACKUP_STANDBY_STR,                 "option-backup-standby")
// CSHANG Replaces MANIFEST_KEY_ARCHIVE_CHECK and INFO_BACKUP_KEY_ARCHIVE_CHECK
STRING_EXTERN(INFO_MANIFEST_KEY_ARCHIVE_CHECK_STR,                  "option-archive-check")
// CSHANG Replaces MANIFEST_KEY_ARCHIVE_COPY and INFO_BACKUP_KEY_ARCHIVE_COPY
STRING_EXTERN(INFO_MANIFEST_KEY_ARCHIVE_COPY_STR,                   "option-archive-check")
// CSHANG Replaces MANIFEST_KEY_ARCHIVE_START and INFO_BACKUP_KEY_ARCHIVE_START
STRING_EXTERN(INFO_MANIFEST_KEY_ARCHIVE_START_STR,                  "backup-archive-start")
// CSHANG Replaces MANIFEST_KEY_ARCHIVE_STOP and INFO_BACKUP_KEY_ARCHIVE_STOP
STRING_EXTERN(INFO_MANIFEST_KEY_ARCHIVE_STOP_STR,                   "backup-archive-stop")
// CSHANG Replaces MANIFEST_KEY_CHECKSUM_PAGE INFO_BACKUP_KEY_CHECKSUM_PAGE
STRING_EXTERN(INFO_MANIFEST_KEY_CHECKSUM_PAGE_STR,                  "option-checksum-page")
// CSHANG Replaces MANIFEST_KEY_COMPRESS and INFO_BACKUP_KEY_COMPRESS
STRING_EXTERN(INFO_MANIFEST_KEY_COMPRESS_STR,                       "option-compress")
// CSHANG Replaces MANIFEST_KEY_HARDLINK and INFO_BACKUP_KEY_HARDLINK
STRING_EXTERN(INFO_MANIFEST_KEY_HARDLINK_STR,                       "option-hardlink")
// CSHANG Replaces MANIFEST_KEY_ONLINE and INFO_BACKUP_KEY_ONLINE
STRING_EXTERN(INFO_MANIFEST_KEY_ONLINE_STR,                         "option-online")
// CSHANG Replaces MANIFEST_KEY_PRIOR and INFO_BACKUP_KEY_PRIOR
STRING_EXTERN(INFO_MANIFEST_KEY_BACKUP_PRIOR_STR,                   "backup-prior")
// CSHANG Replaces MANIFEST_KEY_TIMESTAMP_START and INFO_BACKUP_KEY_TIMESTAMP_START
STRING_EXTERN(INFO_MANIFEST_KEY_TIMESTAMP_START_STR,                "backup-timestamp-start")
// CSHANG Replaces MANIFEST_KEY_TIMESTAMP_STOP and INFO_BACKUP_KEY_TIMESTAMP_STOP
STRING_EXTERN(INFO_MANIFEST_KEY_TIMESTAMP_STOP_STR,                 "backup-timestamp-stop")
// CSHANG Replaces MANIFEST_KEY_TYPE and INFO_BACKUP_KEY_TYPE
STRING_EXTERN(INFO_MANIFEST_KEY_TYPE_STR,                           "backup-type")
