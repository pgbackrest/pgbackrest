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
    unsigned int catalogVersion;
    unsigned int controlVersion;
    uint64_t systemId;
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
InfoPg *infoPgNew(String *fileName, const bool loadFile, const bool ignoreMissing, InfoPgType type);

InfoPgData infoPgDataCurrent(InfoPg *this);
String *infoPgVersionToString(unsigned int version);

void infoPgFree(InfoPg *this);

#endif
