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
infoPgVersionToUIntInternal(const String *version)
{
    unsigned int result = 0;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // If format is not number.number (9.4) or number only (10) then error
        if (!regExpMatchOne(strNew("^[0-9]+[.]*[0-9]+$"), version))
            THROW_FMT(AssertError, "version %s format is invalid", strPtr(version));

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

        // ??? This will be hard to maintain so we should deal with it later
        switch (result)
        {
            case PG_VERSION_83:
            case PG_VERSION_84:
            case PG_VERSION_90:
            case PG_VERSION_91:
            case PG_VERSION_92:
            case PG_VERSION_93:
            case PG_VERSION_94:
            case PG_VERSION_95:
            case PG_VERSION_96:
            case PG_VERSION_10:
            case PG_VERSION_11:
            {
                break;
            }

            default:
                THROW_FMT(AssertError, "version %s is not a valid PostgreSQl version", strPtr(version));
        }
    }
    MEM_CONTEXT_TEMP_END();

    return result;
}

/***********************************************************************************************************************************
Create a new InfoPg object
??? Need to consider adding the following parameters in order to throw errors
        $bRequired,                                 # Is archive info required?  --- may not need this if ignoreMissing is enough
        $bLoad,                                     # Should the file attempt to be loaded?
        $strCipherPassSub,                          # Passphrase to encrypt the subsequent archive files if repo is encrypted
***********************************************************************************************************************************/
InfoPg *
infoPgNew(String *fileName, const bool ignoreMissing, InfoPgType type)
{
    InfoPg *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("infoPg")
    {
        // Create object
        this = memNew(sizeof(InfoPg));
        this->memContext = MEM_CONTEXT_NEW();

        this->info = infoNew(fileName, ignoreMissing);

        Ini *infoPgIni = infoIni(this->info);

        // ??? need to get the history list from the file...for now just getting current
        this->history = lstNew(sizeof(InfoPgData));

        InfoPgData infoPgData = {0};

        MEM_CONTEXT_TEMP_BEGIN()
        {
            String *dbSection = strNew(INFO_SECTION_DB);
            infoPgData.systemId = varUInt64Force(iniGet(infoPgIni, dbSection, strNew(INFO_KEY_DB_SYSTEM_ID)));
            infoPgData.id = (unsigned int)varIntForce(iniGet(infoPgIni, dbSection, strNew(INFO_KEY_DB_ID)));

            String *pgVersion = varStrForce(iniGet(infoPgIni, dbSection, strNew(INFO_KEY_DB_VERSION)));

            // ??? Temporary hack for removing the leading and trailing quotes from pgVersion until can get json parser
            infoPgData.version = infoPgVersionToUIntInternal(strSubN(pgVersion, 1, strSize(pgVersion) - 2));

            if ((type == infoPgBackup) || (type == infoPgManifest))
            {
                infoPgData.catalogVersion =
                    (unsigned int)varIntForce(iniGet(infoPgIni, dbSection, strNew(INFO_KEY_DB_CATALOG_VERSION)));
                infoPgData.controlVersion =
                    (unsigned int)varIntForce(iniGet(infoPgIni, dbSection, strNew(INFO_KEY_DB_CONTROL_VERSION)));
            }
            else if (type != infoPgArchive)
                THROW_FMT(AssertError, "invalid InfoPg type %u", type);
        }
        MEM_CONTEXT_TEMP_END();

        lstAdd(this->history, &infoPgData);
        this->indexCurrent = lstSize(this->history) - 1;
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
String *
infoPgVersionToString(unsigned int version)
{
    return strNewFmt("%u.%u", ((unsigned int)(version/10000)), ((version%10000)/100));
}

/***********************************************************************************************************************************
Free the info
***********************************************************************************************************************************/
void
infoPgFree(InfoPg *this)
{
    if (this != NULL)
        memContextFree(this->memContext);
}
