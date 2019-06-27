/***********************************************************************************************************************************
PostgreSQL Info Handler
***********************************************************************************************************************************/
#include "build.auto.h"

#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "common/debug.h"
#include "common/ini.h"
#include "common/log.h"
#include "common/memContext.h"
#include "common/object.h"
#include "common/type/json.h"
#include "common/type/list.h"
#include "info/info.h"
#include "info/infoPg.h"
#include "postgres/interface.h"
#include "postgres/version.h"
#include "storage/helper.h"

/***********************************************************************************************************************************
Internal constants
***********************************************************************************************************************************/
STRING_STATIC(INFO_SECTION_DB_STR,                                          "db");
STRING_STATIC(INFO_SECTION_DB_HISTORY_STR,                                  "db:history");

VARIANT_STRDEF_EXTERN(INFO_KEY_DB_ID_VAR,                                   INFO_KEY_DB_ID);
VARIANT_STRDEF_STATIC(INFO_KEY_DB_CATALOG_VERSION_VAR,                      "db-catalog-version");
VARIANT_STRDEF_STATIC(INFO_KEY_DB_CONTROL_VERSION_VAR,                      "db-control-version");
VARIANT_STRDEF_STATIC(INFO_KEY_DB_SYSTEM_ID_VAR,                            "db-system-id");
VARIANT_STRDEF_STATIC(INFO_KEY_DB_VERSION_VAR,                              "db-version");

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct InfoPg
{
    MemContext *memContext;                                         // Mem context
    Info *info;                                                     // Info contents
    List *history;                                                  // A list of InfoPgData
    unsigned int historyCurrent;                                    // Index of the current history item
};

OBJECT_DEFINE_FREE(INFO_PG);

/***********************************************************************************************************************************
Create new object
***********************************************************************************************************************************/
InfoPg *
infoPgNew(void)
{
    FUNCTION_LOG_VOID(logLevelTrace);

    InfoPg *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("InfoPg")
    {
        // Create object
        this = memNew(sizeof(InfoPg));
        this->memContext = MEM_CONTEXT_NEW();

        this->info = NULL;

        // Get the pg history list
        this->history = lstNew(sizeof(InfoPgData));

// CSHANG Don't really like this because 0 is index to first history which here is empty. Might be nicer to have -1 and then can alway increment...
        this->historyCurrent = 0;
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_LOG_RETURN(INFO_PG, this);
}

/***********************************************************************************************************************************
Create new object and load contents from a file
***********************************************************************************************************************************/
InfoPg *
infoPgNewLoad(
    const Storage *storage, const String *fileName, InfoPgType type, CipherType cipherType, const String *cipherPass, Ini **ini)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STORAGE, storage);
        FUNCTION_LOG_PARAM(STRING, fileName);
        FUNCTION_LOG_PARAM(ENUM, type);
        FUNCTION_LOG_PARAM(ENUM, cipherType);
        FUNCTION_TEST_PARAM(STRING, cipherPass);
        FUNCTION_LOG_PARAM_P(INI, ini);
    FUNCTION_LOG_END();

    ASSERT(storage != NULL);
    ASSERT(fileName != NULL);
    ASSERT(cipherType == cipherTypeNone || cipherPass != NULL);

    InfoPg *this = infoPgNew();

    MEM_CONTEXT_BEGIN(this->memContext)
    {
        // Load info
        Ini *iniLocal = NULL;
        this->info = infoNewLoad(storage, fileName, cipherType, cipherPass, &iniLocal);

        MEM_CONTEXT_TEMP_BEGIN()
        {
            const StringList *pgHistoryKey = iniSectionKeyList(iniLocal, INFO_SECTION_DB_HISTORY_STR);

            // Get the current history id
            unsigned int pgId = jsonToUInt(iniGet(iniLocal, INFO_SECTION_DB_STR, varStr(INFO_KEY_DB_ID_VAR)));

            // History must include at least one item or the file is corrupt
            ASSERT(strLstSize(pgHistoryKey) > 0);

            // Iterate in reverse because we would like the most recent pg history to be in position 0.  If we need to look at the
            // history list at all we'll be iterating from newest to oldest and putting newest in position 0 makes for more natural
            // looping. Cast the index check to an integer to test for >= 0 (for readability).
            for (unsigned int pgHistoryIdx = strLstSize(pgHistoryKey) - 1; (int)pgHistoryIdx >= 0; pgHistoryIdx--)
            {
                // Load JSON data into a KeyValue
                const KeyValue *pgDataKv = jsonToKv(
                    iniGet(iniLocal, INFO_SECTION_DB_HISTORY_STR, strLstGet(pgHistoryKey, pgHistoryIdx)));

                // Get db values that are common to all info files
                InfoPgData infoPgData =
                {
                    .id = cvtZToUInt(strPtr(strLstGet(pgHistoryKey, pgHistoryIdx))),
                    .version = pgVersionFromStr(varStr(kvGet(pgDataKv, INFO_KEY_DB_VERSION_VAR))),

                    // This is different in archive.info due to a typo that can't be fixed without a format version bump
                    .systemId = varUInt64Force(
                        kvGet(pgDataKv, type == infoPgArchive ? INFO_KEY_DB_ID_VAR : INFO_KEY_DB_SYSTEM_ID_VAR)),
                };

                // Set index if this is the current history item
                if (infoPgData.id == pgId)
                    this->historyCurrent = lstSize(this->history);

                // Get values that are only in backup and manifest files.  These are really vestigial since stanza-create verifies
                // the control and catalog versions so there is no good reason to store them.  However, for backward compatability
                // we must write them at least, even if we give up reading them.
                if (type == infoPgBackup || type == infoPgManifest)
                {
                    infoPgData.catalogVersion = varUIntForce(kvGet(pgDataKv, INFO_KEY_DB_CATALOG_VERSION_VAR));
                    infoPgData.controlVersion = varUIntForce(kvGet(pgDataKv, INFO_KEY_DB_CONTROL_VERSION_VAR));
                }
                else if (type != infoPgArchive)
                    THROW_FMT(AssertError, "invalid InfoPg type %u", type);

                // Using lstAdd because it is more efficient than lstInsert and loading this file is in critical code paths
                lstAdd(this->history, &infoPgData);
            }
        }
        MEM_CONTEXT_TEMP_END();

        if (ini != NULL)
            *ini = iniMove(iniLocal, MEM_CONTEXT_OLD());
    }
    MEM_CONTEXT_END();

    FUNCTION_LOG_RETURN(INFO_PG, this);
}

/***********************************************************************************************************************************
Add Postgres data to the history list at position 0 to ensure the latest history is always first in the list
***********************************************************************************************************************************/
void
infoPgAdd(InfoPg *this, const InfoPgData *infoPgData)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(INFO_PG, this);
        FUNCTION_LOG_PARAM_P(INFO_PG_DATA, infoPgData);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(infoPgData != NULL);

    lstInsert(this->history, 0, infoPgData);
// CSHANG If adding to the list, we need to update the historyCurrent, which means adding it here needs to update the infoPg tests
    this->historyCurrent = 0;

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Set the InfoPg object data based on values passed.
***********************************************************************************************************************************/
InfoPg *
infoPgSet(
    InfoPg *this, InfoPgType type, const unsigned int pgVersion, const uint64_t pgSystemId, const uint32_t pgControlVersion,
    const uint32_t pgCatalogVersion, const String *cipherPassSub)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(INFO_PG, this);
        FUNCTION_LOG_PARAM(ENUM, type);
        FUNCTION_LOG_PARAM(UINT, pgVersion);
        FUNCTION_LOG_PARAM(UINT64, pgSystemId);
        FUNCTION_LOG_PARAM(UINT32, pgControlVersion);
        FUNCTION_TEST_PARAM(UINT32, pgCatalogVersion);
        FUNCTION_TEST_PARAM(STRING, cipherPassSub);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(type == infoPgArchive || (pgControlVersion != 0 && pgCatalogVersion != 0));

    if (infoPgDataTotal(this) == 0)
        this->info = infoNew(cipherPassSub);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        unsigned int pgDataId = 1;

        // If there is some history, then get the historyId of the most current
        if (infoPgDataTotal(this) > 0)
            pgDataId = infoPgCurrentDataId(this) + 1;

        // Set db values that are common to all info files
        InfoPgData infoPgData =
        {
            .id = pgDataId,
            .version = pgVersion,

            // This is different in archive.info due to a typo that can't be fixed without a format version bump
            .systemId = pgSystemId,
        };

        if (type == infoPgBackup || type == infoPgManifest)
        {
            infoPgData.catalogVersion = pgCatalogVersion;
            infoPgData.controlVersion = pgControlVersion;
        }
        else if (type != infoPgArchive)
            THROW_FMT(AssertError, "invalid InfoPg type %u", type);

        infoPgAdd(this, &infoPgData);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(INFO_PG, this);
}

/***********************************************************************************************************************************
Save to file
***********************************************************************************************************************************/
void
infoPgSave(
    InfoPg *this, Ini *ini, const Storage *storage, const String *fileName, InfoPgType type, CipherType cipherType,
    const String *cipherPass)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(INFO_PG, this);
        FUNCTION_LOG_PARAM(INI, ini);
        FUNCTION_LOG_PARAM(STORAGE, storage);
        FUNCTION_LOG_PARAM(STRING, fileName);
        FUNCTION_LOG_PARAM(ENUM, type);
        FUNCTION_LOG_PARAM(ENUM, cipherType);
        FUNCTION_TEST_PARAM(STRING, cipherPass);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(ini != NULL);
    ASSERT(storage != NULL);
    ASSERT(fileName != NULL);
    ASSERT(cipherType == cipherTypeNone || cipherPass != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Set the db section
        InfoPgData pgData = infoPgDataCurrent(this);

        iniSet(ini, INFO_SECTION_DB_STR, varStr(INFO_KEY_DB_ID_VAR), jsonFromUInt(pgData.id));
        iniSet(ini, INFO_SECTION_DB_STR, varStr(INFO_KEY_DB_VERSION_VAR), jsonFromStr(pgVersionToStr(pgData.version)));
        iniSet(ini, INFO_SECTION_DB_STR, varStr(INFO_KEY_DB_SYSTEM_ID_VAR), jsonFromUInt64(pgData.systemId));

        if (type == infoPgBackup || type == infoPgManifest)
        {

            iniSet(ini, INFO_SECTION_DB_STR, varStr(INFO_KEY_DB_CATALOG_VERSION_VAR), jsonFromUInt(pgData.catalogVersion));
            iniSet(ini, INFO_SECTION_DB_STR, varStr(INFO_KEY_DB_CONTROL_VERSION_VAR), jsonFromUInt(pgData.controlVersion));
        }
        else if (type != infoPgArchive)
            THROW_FMT(AssertError, "invalid InfoPg type %u", type);

        // Set the db history section in reverse so oldest history is first instead of last to be consistent with load
        for (unsigned int pgDataIdx = infoPgDataTotal(this) - 1; (int)pgDataIdx >= 0; pgDataIdx--)
        {
            InfoPgData pgData = infoPgData(this, pgDataIdx);

            KeyValue *pgDataKv = kvNew();
            kvPut(pgDataKv, INFO_KEY_DB_VERSION_VAR, VARSTR(pgVersionToStr(pgData.version)));

            if (type == infoPgBackup || type == infoPgManifest)
            {
                kvPut(pgDataKv, INFO_KEY_DB_CATALOG_VERSION_VAR, VARUINT(pgData.catalogVersion));
                kvPut(pgDataKv, INFO_KEY_DB_CONTROL_VERSION_VAR, VARUINT(pgData.controlVersion));
                kvPut(pgDataKv, INFO_KEY_DB_SYSTEM_ID_VAR, VARUINT64(pgData.systemId));
            }
            else
                kvPut(pgDataKv, INFO_KEY_DB_ID_VAR, VARUINT64(pgData.systemId));

            iniSet(ini, INFO_SECTION_DB_HISTORY_STR, varStrForce(VARUINT(pgData.id)), jsonFromKv(pgDataKv, 0));
        }

        infoSave(infoPgInfo(this), ini, storage, fileName, cipherType, cipherPass);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Construct archive id
***********************************************************************************************************************************/
String *
infoPgArchiveId(const InfoPg *this, unsigned int pgDataIdx)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(INFO_PG, this);
        FUNCTION_LOG_PARAM(UINT, pgDataIdx);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    InfoPgData pgData = infoPgData(this, pgDataIdx);

    FUNCTION_LOG_RETURN(STRING, strNewFmt("%s-%u", strPtr(pgVersionToStr(pgData.version)), pgData.id));
}

/***********************************************************************************************************************************
Return the cipher passphrase
***********************************************************************************************************************************/
const String *
infoPgCipherPass(const InfoPg *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(INFO_PG, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(infoCipherPass(this->info));
}

/***********************************************************************************************************************************
Return a structure of the Postgres data from a specific index
***********************************************************************************************************************************/
InfoPgData
infoPgData(const InfoPg *this, unsigned int pgDataIdx)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(INFO_PG, this);
        FUNCTION_LOG_PARAM(UINT, pgDataIdx);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    FUNCTION_LOG_RETURN(INFO_PG_DATA, *((InfoPgData *)lstGet(this->history, pgDataIdx)));
}

/***********************************************************************************************************************************
Return a structure of the current Postgres data
***********************************************************************************************************************************/
InfoPgData
infoPgDataCurrent(const InfoPg *this)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(INFO_PG, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    FUNCTION_LOG_RETURN(INFO_PG_DATA, infoPgData(this, infoPgDataCurrentId(this)));
}

/***********************************************************************************************************************************
Return the current history index
***********************************************************************************************************************************/
unsigned int
infoPgDataCurrentId(const InfoPg *this)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(INFO_PG, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    FUNCTION_LOG_RETURN(UINT, this->historyCurrent);
}

/***********************************************************************************************************************************
Get base info
***********************************************************************************************************************************/
Info *
infoPgInfo(const InfoPg *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(INFO_PG, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->info);
}

/***********************************************************************************************************************************
Return total Postgres data in the history
***********************************************************************************************************************************/
unsigned int
infoPgDataTotal(const InfoPg *this)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(INFO_PG, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    FUNCTION_LOG_RETURN(UINT, lstSize(this->history));
}

/***********************************************************************************************************************************
Return current pgId from the history
***********************************************************************************************************************************/
unsigned int
infoPgCurrentDataId(const InfoPg *this)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(INFO_PG, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    InfoPgData currentData = infoPgDataCurrent(this);

    FUNCTION_LOG_RETURN(UINT, currentData.id);
}

/***********************************************************************************************************************************
Render as string for logging
***********************************************************************************************************************************/
String *
infoPgDataToLog(const InfoPgData *this)
{
    return strNewFmt(
        "{id: %u, version: %u, systemId: %" PRIu64 ", catalogVersion: %" PRIu32 ", controlVersion: %" PRIu32 "}",
        this->id, this->version, this->systemId, this->catalogVersion, this->controlVersion);
}
