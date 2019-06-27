/***********************************************************************************************************************************
Archive Info Handler
***********************************************************************************************************************************/
#ifndef INFO_INFOARCHIVE_H
#define INFO_INFOARCHIVE_H

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
#define INFO_ARCHIVE_TYPE                                           InfoArchive
#define INFO_ARCHIVE_PREFIX                                         infoArchive

typedef struct InfoArchive InfoArchive;

#include "common/crypto/common.h"
#include "common/type/string.h"
#include "info/infoPg.h"
#include "storage/storage.h"

/***********************************************************************************************************************************
Archive info filename
***********************************************************************************************************************************/
#define INFO_ARCHIVE_FILE                                           "archive.info"
#define REGEX_ARCHIVE_DIR_DB_VERSION                                "^[0-9]+(\\.[0-9]+)*-[0-9]+$"

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
InfoArchive *infoArchiveNew(void);
InfoArchive *infoArchiveNewLoad(
    const Storage *storage, const String *fileName, CipherType cipherType, const String *cipherPass);

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
const String *infoArchiveIdHistoryMatch(
    const InfoArchive *this, const unsigned int historyId, const unsigned int pgVersion, const uint64_t pgSystemId);

/***********************************************************************************************************************************
Getters
***********************************************************************************************************************************/
const String *infoArchiveId(const InfoArchive *this);
const String *infoArchiveCipherPass(const InfoArchive *this);
InfoPg *infoArchivePg(const InfoArchive *this);

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
void infoArchiveFree(InfoArchive *this);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_INFO_ARCHIVE_TYPE                                                                                             \
    InfoArchive *
#define FUNCTION_LOG_INFO_ARCHIVE_FORMAT(value, buffer, bufferSize)                                                                \
    objToLog(value, "InfoArchive", buffer, bufferSize)

#endif
