/***********************************************************************************************************************************
PostgreSQL Info Handler
***********************************************************************************************************************************/
#ifndef INFO_INFOPG_H
#define INFO_INFOPG_H

#include <stdint.h>

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct InfoPg InfoPg;

#include "common/crypto/common.h"
#include "common/ini.h"
#include "info/info.h"
#include "storage/storage.h"

/***********************************************************************************************************************************
Constants
***********************************************************************************************************************************/
#define INFO_KEY_DB_ID                                              "db-id"

/***********************************************************************************************************************************
Information about the PostgreSQL cluster
***********************************************************************************************************************************/
typedef struct InfoPgData
{
    unsigned int id;
    uint64_t systemId;
    unsigned int catalogVersion;
    unsigned int version;

    // Control version is required to maintain the file format for older versions of pgBackRest but should not be used elsewhere
    unsigned int controlVersion;
} InfoPgData;

/***********************************************************************************************************************************
Info types for determining data in DB section
***********************************************************************************************************************************/
typedef enum
{
    infoPgArchive = STRID5("archive", 0x16c940e410),                // archive info file
    infoPgBackup = STRID5("backup", 0x21558c220),                   // backup info file
} InfoPgType;

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
FN_EXTERN InfoPg *infoPgNew(InfoPgType type, const String *cipherPassSub);

// Create new object and load contents from a file
FN_EXTERN InfoPg *infoPgNewLoad(IoRead *read, InfoPgType type, InfoLoadNewCallback *callbackFunction, void *callbackData);

/***********************************************************************************************************************************
Getters/Setters
***********************************************************************************************************************************/
typedef struct InfoPgPub
{
    Info *info;                                                     // Info contents
    List *history;                                                  // A list of InfoPgData
} InfoPgPub;

// Archive id
FN_EXTERN String *infoPgArchiveId(const InfoPg *this, unsigned int pgDataIdx);

// Base info
FN_INLINE_ALWAYS Info *
infoPgInfo(const InfoPg *const this)
{
    return THIS_PUB(InfoPg)->info;
}

// Return the cipher passphrase
FN_INLINE_ALWAYS const String *
infoPgCipherPass(const InfoPg *const this)
{
    return infoCipherPass(infoPgInfo(this));
}

// Return current pgId from the history
FN_EXTERN unsigned int infoPgCurrentDataId(const InfoPg *this);

// PostgreSQL info
FN_EXTERN InfoPgData infoPgData(const InfoPg *this, unsigned int pgDataIdx);

// Current PostgreSQL data
FN_EXTERN InfoPgData infoPgDataCurrent(const InfoPg *this);

// Current history index
FN_EXTERN unsigned int infoPgDataCurrentId(const InfoPg *this);

// Total PostgreSQL data in the history
FN_INLINE_ALWAYS unsigned int
infoPgDataTotal(const InfoPg *const this)
{
    return lstSize(THIS_PUB(InfoPg)->history);
}

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Add Postgres data to the history list at position 0 to ensure the latest history is always first in the list
FN_EXTERN void infoPgAdd(InfoPg *this, const InfoPgData *infoPgData);

// Save to IO
FN_EXTERN void infoPgSave(InfoPg *this, IoWrite *write, InfoSaveCallback *callbackFunction, void *callbackData);

// Set the InfoPg object data based on values passed
FN_EXTERN InfoPg *infoPgSet(
    InfoPg *this, InfoPgType type, const unsigned int pgVersion, const uint64_t pgSystemId, const unsigned int pgCatalogVersion);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
FN_EXTERN void infoPgDataToLog(const InfoPgData *this, StringStatic *debugLog);

#define FUNCTION_LOG_INFO_PG_TYPE                                                                                                  \
    InfoPg *
#define FUNCTION_LOG_INFO_PG_FORMAT(value, buffer, bufferSize)                                                                     \
    objNameToLog(value, "InfoPg", buffer, bufferSize)

#define FUNCTION_LOG_INFO_PG_DATA_TYPE                                                                                             \
    InfoPgData
#define FUNCTION_LOG_INFO_PG_DATA_FORMAT(value, buffer, bufferSize)                                                                \
    FUNCTION_LOG_OBJECT_FORMAT(&value, infoPgDataToLog, buffer, bufferSize)

#endif
