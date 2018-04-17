/***********************************************************************************************************************************
InfoDb Handler for database information
***********************************************************************************************************************************/
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "common/memContext.h"
#include "common/ini.h"
#include "common/type/list.h"
#include "info/info.h"
#include "info/infoDb.h"
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
Contains information about the info
***********************************************************************************************************************************/
struct InfoDb
{
    MemContext *memContext;                                         // Context that contains the infoDb
    List *history;                                                  // A list of InfoDbData
    int indexCurrent;                                               // Index to the history list for the db Section
    Info *info;                                                     // Info contents
};

/***********************************************************************************************************************************
Create a new InfoDb object
***********************************************************************************************************************************/
InfoDb *
infoDbNew(String *fileName, const bool loadFile, const bool ignoreMissing, InfoDbType type)
{
// CSHANG the archive.info is missing catalog and control number in DB and History and manifest section name has prefix "backup:"
// and no history section so type here is for determining if we need to add stuff or not. But, could we to modify the files to fix
// up the discrepancies and still be backwards compatible? Or maybe not worry about type here and instead pass type on Save and
// store only things relevant to the type?
    InfoDb *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("infoDb")
    {
        // Create object
        this = memNew(sizeof(InfoDb));
        this->memContext = MEM_CONTEXT_NEW();

        this->info = infoNew(fileName, loadFile, ignoreMissing);

        Ini *infoIni = infoIni(this->info);

        // If the file exists, then get the data into the structure
        if (infoIni->exists)
        {
            // CSHANG Somehow we need to get the history list from the file...for now just dummy code for more than the current
            this->history = lstNew(sizeof(InfoDbData));

            // InfoDbData infoDbData;
            //
            // switch (type)
            // {
            //     case infoDbArchive:
            //     {
            //         lstSort((List *)this, sortAscComparator);
            //         break;
            //     }
            //
            //     case infoDbBackup:
            //     {
            //         infoDbData.dbCatalogVersion =
            //             varInt(iniGet(infoIni, strNew(INFO_SECTION_DB), strNew(INFO_KEY_DB_CATALOG_VERSION)));
            //     // infoDbData.dbCatalogVersion = 201409291;
            //     // infoDbData.dbControlVersion = 942;
            //     // infoDbData.dbSystemId = 6365925855997464783;
            //     // infoDbData.dbVersion = "9.4";
            //     // infoDbData.dbId = 1;
            //         break;
            //     }
            //
            //     case infoDbManifest:
            //     {
            //         lstSort((List *)this, sortDescComparator);
            //         break;
            //     }
            // }



        }
        // If the file doesn't exist, then initialize the data
        else
        {
            this->history = lstNew(sizeof(InfoDbData));  // CSHANG Need to set this somehow - maybe internal function?
            // CSHANG WARNING the archive.info and backup.info history sections differ because the archive.info uses db-id instead of
            // the correct label db-system-id.
            // CSHANG the following is just pseudo code
            // Get db->info
            // ....
            // Fill db section
            // CSHANG create a string - but do we need to free it or create temp mem context?
            String *dbSection = (type == infoDbManifest) ? strNew(INFO_SECTION_DB_MANIFEST) : strNew(INFO_SECTION_DB);

            iniSet(infoIni, dbSection, strNew(INFO_KEY_DB_ID), 1);
            // iniSet(ini, dbSection, strNew(INI_KEY_DB_SYSTEM_ID), someuullnumber);
            // iniSet(ini, dbSection, strNew(INI_KEY_DB_VERSION), someversion);

            // If type == 'backup' or 'manifest then add catalog and control version to DB section
            // If type= 'backup' add catalog and control version to DB History
        }
    }
    MEM_CONTEXT_NEW_END();

    // Return buffer
    return this;
}

/***********************************************************************************************************************************
Free the info
***********************************************************************************************************************************/
void
infoDbFree(InfoDb *this)
{
    if (this != NULL)
        memContextFree(this->memContext);
}
