/***********************************************************************************************************************************
Archive Get File
***********************************************************************************************************************************/
#ifndef COMMAND_ARCHIVE_GET_FILE_H
#define COMMAND_ARCHIVE_GET_FILE_H

#include "common/crypto/common.h"
#include "common/type/string.h"
#include "storage/storage.h"

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Copy a file from the archive to the specified destination
typedef struct ArchiveGetFile
{
    const String *file;                                             // File in the repo (with path, checksum, ext, etc.)
    unsigned int repoIdx;                                           // Repo idx
    const String *archiveId;                                        // Repo archive id
    CipherType cipherType;                                          // Repo cipher type
    const String *cipherPassArchive;                                // Repo archive cipher pass
} ArchiveGetFile;

typedef struct ArchiveGetFileResult
{
    unsigned int actualIdx;                                         // Index of the file from actual list that was retrieved
    StringList *warnList;                                           // Warnings from a successful operation
} ArchiveGetFileResult;

ArchiveGetFileResult archiveGetFile(
    const Storage *storage, const String *request, const List *actualList, const String *walDestination);

#endif
