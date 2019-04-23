/***********************************************************************************************************************************
Manifest Info Handler
***********************************************************************************************************************************/
#ifndef INFO_INFOMANIFEST_H
#define INFO_INFOMANIFEST_H

#include "common/type/variant.h"

/***********************************************************************************************************************************
Constants
***********************************************************************************************************************************/
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

#endif
