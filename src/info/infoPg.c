/***********************************************************************************************************************************
InfoPg Handler for postgres database information
***********************************************************************************************************************************/
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "common/memContext.h"
#include "common/ini.h"
#include "common/memContext.h"
#include "common/regExp.h"
#include "common/type/list.h"
#include "info/info.h"
#include "info/infoPg.h"
#include "postgres/version.h"
#include "storage/helper.h"

/***********************************************************************************************************************************
Internal constants
***********************************************************************************************************************************/
#define INFO_SECTION_DB                                             "db"
#define INFO_SECTION_DB_HISTORY                                     INFO_SECTION_DB ":history"
#define INFO_SECTION_DB_MANIFEST                                    "backup:" INFO_SECTION_DB

#define INFO_KEY_DB_ID                                              "db-id"
#define INFO_KEY_DB_CATALOG_VERSION                                 "db-catalog-version"
#define INFO_KEY_DB_CONTROL_VERSION                                 "db-control-version"
#define INFO_KEY_DB_SYSTEM_ID                                       "db-system-id"
#define INFO_KEY_DB_VERSION                                         "db-version"

/***********************************************************************************************************************************
Contains information about postgres
***********************************************************************************************************************************/
struct InfoPg
{
    MemContext *memContext;                                         // Context that contains the infoPg
    List *history;                                                  // A list of InfoPgData
    unsigned int indexCurrent;                                      // Index to the history list for the db Section
    Info *info;                                                     // Info contents
};


/***********************************************************************************************************************************
Return the PostgreSQL version constant given a string
***********************************************************************************************************************************/
unsigned int
infoPgVersionToUInt(const String *version)
{
    unsigned int result = 0;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // If format is not number.number (9.4) or number only (10) then error
        if (!regExpMatchOne(strNew("^[0-9]+[.]*[0-9]+$"), version))
            THROW(FormatError, "version %s format is invalid", strPtr(version));

        size_t idxStart = (size_t)(strChr(version, '.'));
        int minor = 0;
        int major;

        // If there is a dot, then set the major and minor versions, else just the major
        if ((int)idxStart != -1)
            minor = atoi(strPtr(strSub(version, idxStart + 1)));
        else
            idxStart = strSize(version);

        major = atoi(strPtr(strSubN(version, 0, idxStart)));

        result = (unsigned int)((major * 10000) + (minor * 100));
    }
    MEM_CONTEXT_TEMP_END();

    return result;
}

/***********************************************************************************************************************************
Create a new InfoPg object
***********************************************************************************************************************************/
InfoPg *
infoPgNew(String *fileName, const bool loadFile, const bool ignoreMissing, InfoPgType type)
{
// CSHANG the archive.info is missing catalog and control number in DB and History and manifest section name has prefix "backup:"
// and no history section so type here is for determining if we need to add stuff or not. But, could we to modify the files to fix
// up the discrepancies and still be backwards compatible? Or maybe not worry about type here and instead pass type on Save and
// store only things relevant to the type?
    InfoPg *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("infoPg")
    {
        // Create object
        this = memNew(sizeof(InfoPg));
        this->memContext = MEM_CONTEXT_NEW();

        this->info = infoNew(fileName, loadFile, ignoreMissing);

        Ini *infoPgIni = infoIni(this->info);
        //
        // // If the file exists, then get the data into the structure
        // if (iniFileExists(infoPgIni))
        // {
            // CSHANG Somehow we need to get the history list from the file...for now just getting current
            this->history = lstNew(sizeof(InfoPgData));

            InfoPgData infoPgData = {0};

            MEM_CONTEXT_TEMP_BEGIN()
            {
                String *dbSection = strNew(INFO_SECTION_DB);
                infoPgData.systemId = varUInt64Force(iniGet(infoPgIni, dbSection, strNew(INFO_KEY_DB_SYSTEM_ID)));
                infoPgData.id = (unsigned int)varIntForce(iniGet(infoPgIni, dbSection, strNew(INFO_KEY_DB_ID)));

                infoPgData.version =
                    infoPgVersionToUInt(varStrForce(iniGet(infoPgIni, dbSection, strNew(INFO_KEY_DB_VERSION))));

                if ((type == infoPgBackup) || (type == infoPgManifest))
                {
                    infoPgData.catalogVersion =
                        (unsigned int)varIntForce(iniGet(infoPgIni, dbSection, strNew(INFO_KEY_DB_CATALOG_VERSION)));
                    infoPgData.controlVersion =
                        (unsigned int)varIntForce(iniGet(infoPgIni, dbSection, strNew(INFO_KEY_DB_CONTROL_VERSION)));
                }
            }
            MEM_CONTEXT_TEMP_END();

            lstAdd(this->history, &infoPgData);
            this->indexCurrent = lstSize(this->history) - 1;
        //
        // }
        // // If the file doesn't exist, then initialize the data
        // else
        // {
        //     this->history = lstNew(sizeof(InfoPgData));  // CSHANG Need to set this somehow - maybe internal function?
        //     // CSHANG the archive.info and backup.info history sections differ because the archive.info uses db-id instead of
        //     // the correct label db-system-id.
        //     // Get db->info
        //     // Fill db section
        //     // CSHANG create a string - but do we need to free it or create temp mem context?
        //     String *dbSection = (type == infoPgManifest) ? strNew(INFO_SECTION_DB_MANIFEST) : strNew(INFO_SECTION_DB);
        //
        //     iniSet(infoPgIni, dbSection, strNew(INFO_KEY_DB_ID), 1);
        //     // iniSet(ini, dbSection, strNew(INI_KEY_DB_SYSTEM_ID), someuullnumber);
        //     // iniSet(ini, dbSection, strNew(INI_KEY_DB_VERSION), someversion);
        //
        //     // If type == 'backup' or 'manifest then add catalog and control version to DB section
        //     // If type == 'backup' (only) add catalog and control version to DB History
        // }

    }
    MEM_CONTEXT_NEW_END();

    // Return buffer
    return this;
}

/***********************************************************************************************************************************
Return a structure of the current Postgres data
***********************************************************************************************************************************/
InfoPgData
infoPgDataCurrent(InfoPg *this)
{
    return *((InfoPgData *)lstGet(this->history, this->indexCurrent));
}


/***********************************************************************************************************************************
Return a string representation of the PostgreSQL version
***********************************************************************************************************************************/
// String *
// infoPgVersionToString(String *version)
// {
//     return (unsigned int)((atoi(strPtr(strSub(version, strChr(version, '.') + 1))) * 100)
//         + (atoi(strPtr(strTrunc(version, strChr(version, '.')))) * 10000));
//     // int idxDot = strChr(version, '.');
//     // int minor = atoi(strPtr(strSub(version, idxDot + 1)));
//     // int major = atoi(strPtr(strTrunc(version, idxDot)));
//     //
//     // return (unsigned int)((major * 10000) + (minor * 100));
// }

/***********************************************************************************************************************************
Free the info
***********************************************************************************************************************************/
void
infoPgFree(InfoPg *this)
{
    if (this != NULL)
        memContextFree(this->memContext);
}
