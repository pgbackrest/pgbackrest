/***********************************************************************************************************************************
Archive Push File
***********************************************************************************************************************************/
#ifndef COMMAND_ARCHIVE_PUSH_FILE_H
#define COMMAND_ARCHIVE_PUSH_FILE_H

#include "common/compress/helper.h"
#include "common/crypto/common.h"
#include "common/type/string.h"
#include "storage/storage.h"

/***********************************************************************************************************************************
Structure to hold information for each repository the archive file will be pushed to. An array of these must be passed to
archivePushFile() with size equal to cfgOptionGroupIdxTotal(cfgOptGrpRepo).
***********************************************************************************************************************************/
typedef struct ArchivePushFileRepoData
{
    const String *archiveId;
    CipherType cipherType;
    const String *cipherPass;
    CompressType compressType;
    int compressLevel;
} ArchivePushFileRepoData;

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Copy a file from the source to the archive
String *archivePushFile(
    const String *walSource, unsigned int pgVersion, uint64_t pgSystemId, const String *archiveFile,
    const ArchivePushFileRepoData *repoData);

#endif
