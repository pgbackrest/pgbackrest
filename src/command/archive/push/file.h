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
Functions
***********************************************************************************************************************************/
// Copy a file from the source to the archive
String *archivePushFile(
    const String *walSource, const String *archiveId, unsigned int pgVersion, uint64_t pgSystemId, const String *archiveFile,
    CipherType cipherType, const String *cipherPass, CompressType compressType, int compressLevel);

#endif
