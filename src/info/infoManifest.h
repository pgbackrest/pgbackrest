/***********************************************************************************************************************************
Manifest Info Handler
***********************************************************************************************************************************/
#ifndef INFO_INFOMANIFEST_H
#define INFO_INFOMANIFEST_H

/* CSHANG It is not good to have a dependency on a config name so they've been removed. We have renamed config options before and
it's OK to affect the config file but changes to the info files would require a format version bump. option-hardlink is a prime
example as the config option name changed to repo-hardlink
*/
// CSHANG Replaces MANIFEST_KEY_BACKUP_STANDBY and INFO_BACKUP_KEY_BACKUP_STANDBY
#define INFO_MANIFEST_KEY_BACKUP_STANDBY()                                                                                         \
    strNew("option-backup-standby")
// CSHANG Replaces MANIFEST_KEY_ARCHIVE_CHECK and INFO_BACKUP_KEY_ARCHIVE_CHECK
#define INFO_MANIFEST_KEY_ARCHIVE_CHECK()                                                                                          \
    strNew("option-archive-check")
// CSHANG Replaces MANIFEST_KEY_ARCHIVE_COPY and INFO_BACKUP_KEY_ARCHIVE_COPY
#define INFO_MANIFEST_KEY_ARCHIVE_COPY()                                                                                           \
    strNew("option-archive-check")
// CSHANG Replaces MANIFEST_KEY_ARCHIVE_START and INFO_BACKUP_KEY_ARCHIVE_START
#define INFO_MANIFEST_KEY_ARCHIVE_START()                                                                                          \
    strNew("backup-archive-start")
// CSHANG Replaces MANIFEST_KEY_ARCHIVE_STOP and INFO_BACKUP_KEY_ARCHIVE_STOP
#define INFO_MANIFEST_KEY_ARCHIVE_STOP()                                                                                           \
    strNew("backup-archive-stop")
// CSHANG Replaces MANIFEST_KEY_CHECKSUM_PAGE INFO_BACKUP_KEY_CHECKSUM_PAGE
#define INFO_MANIFEST_KEY_CHECKSUM_PAGE()                                                                                          \
    strNew("option-checksum-page")
// CSHANG Replaces MANIFEST_KEY_COMPRESS and INFO_BACKUP_KEY_COMPRESS
#define INFO_MANIFEST_KEY_COMPRESS()                                                                                               \
    strNew("option-compress")
// CSHANG Replaces MANIFEST_KEY_HARDLINK and INFO_BACKUP_KEY_HARDLINK
#define INFO_MANIFEST_KEY_HARDLINK()                                                                                               \
    strNew("option-hardlink")
// CSHANG Replaces MANIFEST_KEY_ONLINE and INFO_BACKUP_KEY_ONLINE
#define INFO_MANIFEST_KEY_ONLINE()                                                                                                 \
    strNew("option-online")
// CSHANG Replaces MANIFEST_KEY_TIMESTAMP_START and INFO_BACKUP_KEY_TIMESTAMP_START
#define INFO_MANIFEST_KEY_TIMESTAMP_START()                                                                                        \
    strNew("backup-timestamp-start")
// CSHANG Replaces MANIFEST_KEY_TIMESTAMP_STOP and INFO_BACKUP_KEY_TIMESTAMP_STOP
#define INFO_MANIFEST_KEY_TIMESTAMP_STOP()                                                                                         \
    strNew("backup-timestamp-stop")
// CSHANG Replaces MANIFEST_KEY_TYPE and INFO_BACKUP_KEY_TYPE
#define INFO_MANIFEST_KEY_TYPE()                                                                                                   \
    strNew("backup-type")
