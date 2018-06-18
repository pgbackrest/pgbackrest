/***********************************************************************************************************************************
InfoPg Handler for postgres database information
***********************************************************************************************************************************/
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "common/debug.h"
#include "common/log.h"
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
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(STRING, version);

        FUNCTION_DEBUG_ASSERT(version != NULL);
    FUNCTION_DEBUG_END();

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

    FUNCTION_DEBUG_RESULT(UINT, result);
}

/***********************************************************************************************************************************
Create a new InfoPg object
??? Need to consider adding the following parameters in order to throw errors
        $bRequired,                                 # Is archive info required?  --- may not need this if ignoreMissing is enough
        $bLoad,                                     # Should the file attempt to be loaded?
        $strCipherPassSub,                          # Passphrase to encrypt the subsequent archive files if repo is encrypted
??? Currently this assumes the file exists and loads data from it
***********************************************************************************************************************************/
InfoPg *
infoPgNew(String *fileName, const bool ignoreMissing, InfoPgType type)
{
    FUNCTION_DEBUG_BEGIN(logLevelDebug);
        FUNCTION_DEBUG_PARAM(STRING, fileName);
        FUNCTION_DEBUG_PARAM(BOOL, ignoreMissing);

        FUNCTION_DEBUG_ASSERT(fileName != NULL);
    FUNCTION_DEBUG_END();

    InfoPg *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("infoPg")
    {
        // Create object
        this = memNew(sizeof(InfoPg));
        this->memContext = MEM_CONTEXT_NEW();

        this->info = infoNew(fileName, ignoreMissing);

        // ??? If file does not exist, then do nothing - infoExists(this->info) will be false, the user will have to set infoPgData
        Ini *infoPgIni = infoIni(this->info);

        // ??? need to get the history list from the file in ascending order but need json parser so for now just getting current
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

        infoPgAdd(this, &infoPgData);

    }
    MEM_CONTEXT_NEW_END();

    // Return buffer
    FUNCTION_DEBUG_RESULT(INFO_PG, this);
}

/***********************************************************************************************************************************
Add Postgres data tot he history list and return the new currentIndex
***********************************************************************************************************************************/
unsigned int
infoPgAdd(InfoPg *this, InfoPgData *infoPgData)
{
    FUNCTION_DEBUG_BEGIN(logLevelDebug);  // CSHANG Maybe change to trace?
        FUNCTION_DEBUG_PARAM(INFO_PG, this);
        FUNCTION_DEBUG_PARAM(INFO_PG_DATAP, infoPgData);

        FUNCTION_DEBUG_ASSERT(this != NULL);
        FUNCTION_DEBUG_ASSERT(infoPgData != NULL);
    FUNCTION_DEBUG_END();

    lstAdd(this->history, infoPgData);

    // CSHANG this assumes added in ascending order, but when reading the history from the file, can we ensure that? May be better
    // to loop throuh the history and get the highest ID value?
    this->indexCurrent = lstSize(this->history) - 1;

    FUNCTION_DEBUG_RESULT(UINT, this->indexCurrent);  // CSHANG Do we need to return the index? Nice for debugging...
}

/***********************************************************************************************************************************
Return a structure of the current Postgres data
***********************************************************************************************************************************/
InfoPgData
infoPgDataCurrent(InfoPg *this)
{
    FUNCTION_DEBUG_BEGIN(logLevelDebug);  // CSHANG Maybe change to trace?
        FUNCTION_DEBUG_PARAM(INFO_PG, this);

        FUNCTION_DEBUG_ASSERT(this != NULL);
    FUNCTION_DEBUG_END();

    FUNCTION_DEBUG_RESULT(INFO_PG_DATA, *((InfoPgData *)lstGet(this->history, this->indexCurrent)));
}

/***********************************************************************************************************************************
Return a string representation of the PostgreSQL version
***********************************************************************************************************************************/
String *
infoPgVersionToString(unsigned int version)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(UINT, version);
    FUNCTION_DEBUG_END();

    FUNCTION_DEBUG_RESULT(STRING, strNewFmt("%u.%u", ((unsigned int)(version/10000)), ((version%10000)/100)));
}

/***********************************************************************************************************************************
Convert to a zero-terminated string for logging
***********************************************************************************************************************************/
size_t
infoPgDataToLog(const InfoPgData *this, char *buffer, size_t bufferSize)
{
    size_t result = 0;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        String *string = NULL;

        if (this == NULL)
            string = strNew("null");
        else
            string = strNewFmt("{\"id: %u, version: %u, systemId %" PRIu64 ", catalog %"PRIu32", control %"PRIu32"\"}",
                this->id, this->version, this->systemId, this->catalogVersion, this->controlVersion);

        result = (size_t)snprintf(buffer, bufferSize, "%s", strPtr(string));
    }
    MEM_CONTEXT_TEMP_END();

    return result;
}

/***********************************************************************************************************************************
Free the info
***********************************************************************************************************************************/
void
infoPgFree(InfoPg *this)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(INFO_PG, this);
    FUNCTION_DEBUG_END();

    if (this != NULL)
        memContextFree(this->memContext);

    FUNCTION_DEBUG_RESULT_VOID();
}
