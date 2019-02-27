/***********************************************************************************************************************************
Archive Get File
***********************************************************************************************************************************/
#ifndef COMMAND_ARCHIVE_GET_FILE_H
#define COMMAND_ARCHIVE_GET_FILE_H

#include "common/type/string.h"
#include "crypto/crypto.h"
#include "storage/storage.h"

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
int archiveGetFile(
    const Storage *storage, const String *archiveFile, const String *walDestination, bool durable, CipherType cipherType,
    const String *cipherPass);

#endif
