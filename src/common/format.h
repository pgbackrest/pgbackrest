/***********************************************************************************************************************************
Repository Format

Defines the format for info and manifest files as well as on-disk structure. Each info file and manifest stores the format it was
written with, so a repository may contain more than one format while older backups and archives expire.

A constant is defined for each format so that code which varies by format can be explicit about the format it applies to.
REPOSITORY_FORMAT_MIN/MAX are the range that can be read. The allow list for repo-format in build/config.yaml must be kept in sync
with MIN/MAX and its default is the format used for new repositories.
***********************************************************************************************************************************/
#ifndef COMMON_FORMAT_H
#define COMMON_FORMAT_H

#include "common/crypto/common.h"

/***********************************************************************************************************************************
Format numbers
***********************************************************************************************************************************/
#define REPOSITORY_FORMAT_5                                         5
#define REPOSITORY_FORMAT_6                                         6

#define REPOSITORY_FORMAT_MIN                                       REPOSITORY_FORMAT_5
#define REPOSITORY_FORMAT_MAX                                       REPOSITORY_FORMAT_6

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Error when a format cannot be read by this version. The format in a cipher header is checked before anything is decrypted, since
// decrypting requires knowing what the format expects and this version does not know what a newer format expects.
FN_EXTERN void repoFormatValidate(unsigned int format);

// Digest a pass stored in a file at this format derives the key with. SHA-1 is what every repository used before format 6 and is
// kept for those, so a repository that has not been migrated is read and written exactly as it was. A pass is generated with the
// digest of the file it will be stored in, since that is what a reader will derive it with.
FN_EXTERN HashType repoFormatDigest(unsigned int format);

#endif
