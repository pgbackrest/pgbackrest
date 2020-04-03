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
int archiveGetFile(
    const Storage *storage, const String *archiveFile, const String *walDestination, bool durable, CipherType cipherType,
    const String *cipherPass);

#endif
