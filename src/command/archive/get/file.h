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
typedef struct ArchiveGetFile
{
    const String *file;                                             // File in the repo (with path, checksum, ext, etc.)
    unsigned int repoIdx;                                           // Repo idx
    const String *archiveId;                                        // Repo archive id
    CipherType cipherType;                                          // Repo cipher type
    const String *cipherPassArchive;                                // Repo archive cipher pass
} ArchiveGetFile;

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Copy a file from the archive to the specified destination
void archiveGetFile(
    const Storage *storage, const String *request, const List *actualList, const String *walDestination, bool durable);

#endif
