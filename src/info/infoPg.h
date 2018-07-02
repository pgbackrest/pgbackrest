/***********************************************************************************************************************************
InfoPg Handler for pgbackrest information
***********************************************************************************************************************************/
#ifndef INFO_INFOPG_H
#define INFO_INFOPG_H

#include <stdint.h>

/***********************************************************************************************************************************
InfoPg object
***********************************************************************************************************************************/
typedef struct InfoPg InfoPg;

/***********************************************************************************************************************************
InfoPg Postgres data object
***********************************************************************************************************************************/
typedef struct InfoPgData
{
    unsigned int id;
    uint64_t systemId;
    uint32_t catalogVersion;
    uint32_t controlVersion;
    unsigned int version;
} InfoPgData;

/***********************************************************************************************************************************
Info types for determining data in DB section
***********************************************************************************************************************************/
typedef enum
{
    infoPgArchive,                                                  // archive info file
    infoPgBackup,                                                   // backup info file
    infoPgManifest,                                                 // manifest file
} InfoPgType;

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
InfoPg *infoPgNew(const String *fileName, InfoPgType type);
unsigned int infoPgAdd(InfoPg *this, const InfoPgData *infoPgData);
InfoPgData infoPgDataCurrent(const InfoPg *this);
String *infoPgVersionToString(unsigned int version);

void infoPgFree(InfoPg *this);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
size_t infoPgDataToLog(const InfoPgData *this, char *buffer, size_t bufferSize);

#define FUNCTION_DEBUG_INFO_PG_TYPE                                                                                                \
    InfoPg *
#define FUNCTION_DEBUG_INFO_PG_FORMAT(value, buffer, bufferSize)                                                                   \
    objToLog(value, "InfoPg", buffer, bufferSize)
#define FUNCTION_DEBUG_INFO_PG_DATA_TYPE                                                                                           \
    InfoPgData
#define FUNCTION_DEBUG_INFO_PG_DATA_FORMAT(value, buffer, bufferSize)                                                              \
    infoPgDataToLog(&value, buffer, bufferSize)
#define FUNCTION_DEBUG_INFO_PG_DATAP_TYPE                                                                                          \
    InfoPgData *
#define FUNCTION_DEBUG_INFO_PG_DATAP_FORMAT(value, buffer, bufferSize)                                                             \
    infoPgDataToLog(value, buffer, bufferSize)


#endif
