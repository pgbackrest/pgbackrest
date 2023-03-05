/***********************************************************************************************************************************
Archive Info Handler
***********************************************************************************************************************************/
#ifndef INFO_INFOARCHIVE_H
#define INFO_INFOARCHIVE_H

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct InfoArchive InfoArchive;

#include "common/crypto/common.h"
#include "common/type/object.h"
#include "common/type/string.h"
#include "info/infoPg.h"
#include "storage/storage.h"

/***********************************************************************************************************************************
Archive info filename
***********************************************************************************************************************************/
#define INFO_ARCHIVE_FILE                                           "archive.info"
#define REGEX_ARCHIVE_DIR_DB_VERSION                                "^[0-9]+(\\.[0-9]+)*-[0-9]+$"

#define INFO_ARCHIVE_PATH_FILE                                      STORAGE_REPO_ARCHIVE "/" INFO_ARCHIVE_FILE
STRING_DECLARE(INFO_ARCHIVE_PATH_FILE_STR);
#define INFO_ARCHIVE_PATH_FILE_COPY                                 INFO_ARCHIVE_PATH_FILE INFO_COPY_EXT
STRING_DECLARE(INFO_ARCHIVE_PATH_FILE_COPY_STR);

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
FN_EXTERN InfoArchive *infoArchiveNew(const unsigned int pgVersion, const uint64_t pgSystemId, const String *cipherPassSub);

// Create new object and load contents from IoRead
FN_EXTERN InfoArchive *infoArchiveNewLoad(IoRead *read);

/***********************************************************************************************************************************
Getters/Setters
***********************************************************************************************************************************/
typedef struct InfoArchivePub
{
    MemContext *memContext;                                         // Mem context
    InfoPg *infoPg;                                                 // Contents of the DB data
} InfoArchivePub;

// PostgreSQL info
FN_INLINE_ALWAYS InfoPg *
infoArchivePg(const InfoArchive *const this)
{
    return THIS_PUB(InfoArchive)->infoPg;
}

FN_EXTERN InfoArchive *infoArchivePgSet(InfoArchive *this, unsigned int pgVersion, uint64_t pgSystemId);

// Current archive id
FN_INLINE_ALWAYS const String *
infoArchiveId(const InfoArchive *const this)
{
    return infoPgArchiveId(infoArchivePg(this), infoPgDataCurrentId(infoArchivePg(this)));
}

// Cipher passphrase
FN_INLINE_ALWAYS const String *
infoArchiveCipherPass(const InfoArchive *const this)
{
    return infoPgCipherPass(infoArchivePg(this));
}

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Given a backrest history id and postgres systemId and version, return the archiveId of the best match
FN_EXTERN const String *infoArchiveIdHistoryMatch(
    const InfoArchive *this, const unsigned int historyId, const unsigned int pgVersion, const uint64_t pgSystemId);

// Move to a new parent mem context
FN_INLINE_ALWAYS InfoArchive *
infoArchiveMove(InfoArchive *const this, MemContext *const parentNew)
{
    return objMove(this, parentNew);
}

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
FN_INLINE_ALWAYS void
infoArchiveFree(InfoArchive *const this)
{
    objFree(this);
}

/***********************************************************************************************************************************
Helper functions
***********************************************************************************************************************************/
// Load archive info
FN_EXTERN InfoArchive *infoArchiveLoadFile(
    const Storage *storage, const String *fileName, CipherType cipherType, const String *cipherPass);

// Save archive info
FN_EXTERN void infoArchiveSaveFile(
    InfoArchive *infoArchive, const Storage *storage, const String *fileName, CipherType cipherType, const String *cipherPass);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_INFO_ARCHIVE_TYPE                                                                                             \
    InfoArchive *
#define FUNCTION_LOG_INFO_ARCHIVE_FORMAT(value, buffer, bufferSize)                                                                \
    objNameToLog(value, "InfoArchive", buffer, bufferSize)

#endif
