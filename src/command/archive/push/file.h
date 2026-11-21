/***********************************************************************************************************************************
Archive Push File
***********************************************************************************************************************************/
#ifndef COMMAND_ARCHIVE_PUSH_FILE_H
#define COMMAND_ARCHIVE_PUSH_FILE_H

#include "common/compress/helper.h"
#include "common/crypto/spec.h"
#include "common/type/string.h"
#include "storage/storage.h"

/***********************************************************************************************************************************
Structure to hold information for each repository the archive file will be pushed to. An array of these must be passed to
archivePushFile() with size equal to cfgOptionGroupIdxTotal(cfgOptGrpRepo).
***********************************************************************************************************************************/
typedef struct ArchivePushFileRepoData
{
    unsigned int repoIdx;
    const String *archiveId;
    unsigned int format;                                            // Repo format
    const CipherSpec *cipherSpecArchive;                            // Repo archive cipher spec
    const String *cipherIdArchive;                                  // Archive key id, NULL when no id
} ArchivePushFileRepoData;

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
typedef struct ArchivePushFileRepoError
{
    unsigned int repoIdx;                                           // Repo index
    const String *message;                                          // Error message
} ArchivePushFileRepoError;

typedef struct ArchivePushFileResult
{
    StringList *warnList;                                           // Warnings from a successful operation
    String *error;                                                  // Prior errors and repo errors, NULL when there are none
    List *repoErrorList;                                            // Errors from repos that failed (ArchivePushFileRepoError)
} ArchivePushFileResult;

// Copy a file from the source to the archive
FN_EXTERN ArchivePushFileResult archivePushFile(
    const String *walSource, bool headerCheck, bool modeCheck, unsigned int pgVersion, uint64_t pgSystemId,
    const String *archiveFile, CompressType compressType, int compressLevel, const List *repoList,
    const StringList *priorErrorList);

#endif
