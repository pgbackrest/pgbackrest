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
    VARIANT_DECLARE(INFO_KEY_DB_ID_VAR);

/***********************************************************************************************************************************
Information about the PostgreSQL cluster
***********************************************************************************************************************************/
typedef struct InfoPgData
{
    unsigned int id;
    uint64_t systemId;
    unsigned int catalogVersion;
    unsigned int version;
} InfoPgData;

/***********************************************************************************************************************************
Info types for determining data in DB section
***********************************************************************************************************************************/
typedef enum
{
    infoPgArchive,                                                  // archive info file
    infoPgBackup,                                                   // backup info file
} InfoPgType;

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
InfoPg *infoPgNew(InfoPgType type, const String *cipherPassSub);

// Create new object and load contents from a file
InfoPg *infoPgNewLoad(IoRead *read, InfoPgType type, InfoLoadNewCallback *callbackFunction, void *callbackData);

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Add Postgres data to the history list at position 0 to ensure the latest history is always first in the list
void infoPgAdd(InfoPg *this, const InfoPgData *infoPgData);

// Save to IO
void infoPgSave(InfoPg *this, IoWrite *write, InfoSaveCallback *callbackFunction, void *callbackData);

// Set the InfoPg object data based on values passed
InfoPg *infoPgSet(
    InfoPg *this, InfoPgType type, const unsigned int pgVersion, const uint64_t pgSystemId, const unsigned int pgCatalogVersion);

/***********************************************************************************************************************************
Getters/Setters
***********************************************************************************************************************************/
// Archive id
String *infoPgArchiveId(const InfoPg *this, unsigned int pgDataIdx);

// Return the cipher passphrase
const String *infoPgCipherPass(const InfoPg *this);

// Return current pgId from the history
unsigned int infoPgCurrentDataId(const InfoPg *this);

// PostgreSQL info
InfoPgData infoPgData(const InfoPg *this, unsigned int pgDataIdx);

// Current PostgreSQL data
InfoPgData infoPgDataCurrent(const InfoPg *this);

// Current history index
unsigned int infoPgDataCurrentId(const InfoPg *this);

// Total PostgreSQL data in the history
unsigned int infoPgDataTotal(const InfoPg *this);

// Base info
Info *infoPgInfo(const InfoPg *this);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
String *infoPgDataToLog(const InfoPgData *this);

#define FUNCTION_LOG_INFO_PG_TYPE                                                                                                  \
    InfoPg *
#define FUNCTION_LOG_INFO_PG_FORMAT(value, buffer, bufferSize)                                                                     \
    objToLog(value, "InfoPg", buffer, bufferSize)

#define FUNCTION_LOG_INFO_PG_DATA_TYPE                                                                                             \
    InfoPgData
#define FUNCTION_LOG_INFO_PG_DATA_FORMAT(value, buffer, bufferSize)                                                                \
    FUNCTION_LOG_STRING_OBJECT_FORMAT(&value, infoPgDataToLog, buffer, bufferSize)


#endif
