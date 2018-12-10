/***********************************************************************************************************************************
PostgreSQL Info Handler
***********************************************************************************************************************************/
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "common/assert.h"
#include "common/debug.h"
#include "common/log.h"
#include "common/memContext.h"
#include "common/ini.h"
#include "common/memContext.h"
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
STRING_STATIC(INFO_SECTION_DB_HISTORY_STR,                          "db:history");

STRING_STATIC(INFO_KEY_DB_ID_STR,                                   "db-id");
STRING_STATIC(INFO_KEY_DB_CATALOG_VERSION_STR,                      "db-catalog-version");
STRING_STATIC(INFO_KEY_DB_CONTROL_VERSION_STR,                      "db-control-version");
STRING_STATIC(INFO_KEY_DB_SYSTEM_ID_STR,                            "db-system-id");
STRING_STATIC(INFO_KEY_DB_VERSION_STR,                              "db-version");

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct InfoPg
{
    MemContext *memContext;                                         // Context that contains the infoPg
    List *history;                                                  // A list of InfoPgData
    Info *info;                                                     // Info contents
};

/***********************************************************************************************************************************
Load an InfoPg object
??? Need to consider adding the following parameters in order to throw errors
        $bRequired,                                 # Is archive info required?  --- may not need this if ignoreMissing is enough
        $bLoad,                                     # Should the file attempt to be loaded?
        $strCipherPassSub,                          # Passphrase to encrypt the subsequent archive files if repo is encrypted
??? Currently this assumes the file exists and loads data from it
***********************************************************************************************************************************/
InfoPg *
infoPgNew(const Storage *storage, const String *fileName, InfoPgType type, CipherType cipherType, const String *cipherPass)
{
    FUNCTION_DEBUG_BEGIN(logLevelDebug);
        FUNCTION_DEBUG_PARAM(STORAGE, storage);
        FUNCTION_DEBUG_PARAM(STRING, fileName);
        FUNCTION_DEBUG_PARAM(ENUM, type);
        FUNCTION_DEBUG_PARAM(ENUM, cipherType);
        // cipherPass omitted for security

        FUNCTION_DEBUG_ASSERT(fileName != NULL);
    FUNCTION_DEBUG_END();

    InfoPg *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("infoPg")
    {
        // Create object
        this = memNew(sizeof(InfoPg));
        this->memContext = MEM_CONTEXT_NEW();
        this->info = infoNew(storage, fileName, cipherType, cipherPass);

        // Get the pg history list
        this->history = lstNew(sizeof(InfoPgData));

        MEM_CONTEXT_TEMP_BEGIN()
        {
            const Ini *infoPgIni = infoIni(this->info);
            const String *pgHistorySection = INFO_SECTION_DB_HISTORY_STR;
            const StringList *pgHistoryKey = iniSectionKeyList(infoPgIni, pgHistorySection);
            const Variant *idKey = varNewStr(INFO_KEY_DB_ID_STR);
            const Variant *systemIdKey = varNewStr(INFO_KEY_DB_SYSTEM_ID_STR);
            const Variant *versionKey = varNewStr(INFO_KEY_DB_VERSION_STR);

            // History must include at least one item or the file is corrupt
            ASSERT(strLstSize(pgHistoryKey) > 0);

            // Iterate in reverse because we would like the most recent pg history to be in position 0.  If we need to look at the
            // history list at all we'll be iterating from newest to oldest and putting newest in position 0 makes for more natural
            // looping.
            for (unsigned int pgHistoryIdx = strLstSize(pgHistoryKey) - 1; pgHistoryIdx < strLstSize(pgHistoryKey); pgHistoryIdx--)
            {
                // Load JSON data into a KeyValue
                const KeyValue *pgDataKv = jsonToKv(
                    varStr(iniGet(infoPgIni, pgHistorySection, strLstGet(pgHistoryKey, pgHistoryIdx))));

                // Get db values that are common to all info files
                InfoPgData infoPgData =
                {
                    .id = cvtZToUInt(strPtr(strLstGet(pgHistoryKey, pgHistoryIdx))),
                    .version = pgVersionFromStr(varStr(kvGet(pgDataKv, versionKey))),

                    // This is different in archive.info due to a typo that can't be fixed without a format version bump
                    .systemId = varUInt64Force(kvGet(pgDataKv, type == infoPgArchive ? idKey : systemIdKey)),
                };

                // Get values that are only in backup and manifest files.  These are really vestigial since stanza-create verifies
                // the control and catalog versions so there is no good reason to store them.  However, for backward compatability
                // we must write them at least, even if we give up reading them.
                if (type == infoPgBackup || type == infoPgManifest)
                {
                    const Variant *catalogVersionKey = varNewStr(INFO_KEY_DB_CATALOG_VERSION_STR);
                    const Variant *controlVersionKey = varNewStr(INFO_KEY_DB_CONTROL_VERSION_STR);

                    infoPgData.catalogVersion = (unsigned int)varUInt64Force(kvGet(pgDataKv, catalogVersionKey));
                    infoPgData.controlVersion = (unsigned int)varUInt64Force(kvGet(pgDataKv, controlVersionKey));
                }
                else if (type != infoPgArchive)
                    THROW_FMT(AssertError, "invalid InfoPg type %u", type);

                lstAdd(this->history, &infoPgData);
            }
        }
        MEM_CONTEXT_TEMP_END();
    }
    MEM_CONTEXT_NEW_END();

    // Return buffer
    FUNCTION_DEBUG_RESULT(INFO_PG, this);
}

/***********************************************************************************************************************************
Add Postgres data to the history list at position 0 to ensure the latest history is always first in the list
***********************************************************************************************************************************/
void
infoPgAdd(InfoPg *this, const InfoPgData *infoPgData)
{
    FUNCTION_DEBUG_BEGIN(logLevelDebug);
        FUNCTION_DEBUG_PARAM(INFO_PG, this);
        FUNCTION_DEBUG_PARAM(INFO_PG_DATAP, infoPgData);

        FUNCTION_DEBUG_ASSERT(this != NULL);
        FUNCTION_DEBUG_ASSERT(infoPgData != NULL);
    FUNCTION_DEBUG_END();

    lstInsert(this->history, 0, infoPgData);

    FUNCTION_DEBUG_RESULT_VOID();
}

/***********************************************************************************************************************************
Construct archive id
***********************************************************************************************************************************/
String *
infoPgArchiveId(const InfoPg *this, unsigned int pgDataIdx)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(INFO_PG, this);
        FUNCTION_DEBUG_PARAM(UINT, pgDataIdx);

        FUNCTION_DEBUG_ASSERT(this != NULL);
    FUNCTION_DEBUG_END();

    InfoPgData pgData = infoPgData(this, pgDataIdx);

    FUNCTION_DEBUG_RESULT(STRING, strNewFmt("%s-%u", strPtr(pgVersionToStr(pgData.version)), pgData.id));
}

/***********************************************************************************************************************************
Return the cipher passphrase
***********************************************************************************************************************************/
const String *
infoPgCipherPass(const InfoPg *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(INFO_PG, this);

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RESULT(CONST_STRING, infoCipherPass(this->info));
}

/***********************************************************************************************************************************
Return a structure of the Postgres data from a specific index
***********************************************************************************************************************************/
InfoPgData
infoPgData(const InfoPg *this, unsigned int pgDataIdx)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(INFO_PG, this);
        FUNCTION_DEBUG_PARAM(UINT, pgDataIdx);

        FUNCTION_DEBUG_ASSERT(this != NULL);
    FUNCTION_DEBUG_END();

    FUNCTION_DEBUG_RESULT(INFO_PG_DATA, *((InfoPgData *)lstGet(this->history, pgDataIdx)));
}

/***********************************************************************************************************************************
Return a structure of the current Postgres data
***********************************************************************************************************************************/
InfoPgData
infoPgDataCurrent(const InfoPg *this)
{
    FUNCTION_DEBUG_BEGIN(logLevelDebug);
        FUNCTION_DEBUG_PARAM(INFO_PG, this);

        FUNCTION_DEBUG_ASSERT(this != NULL);
    FUNCTION_DEBUG_END();

    FUNCTION_DEBUG_RESULT(INFO_PG_DATA, infoPgData(this, 0));
}

/***********************************************************************************************************************************
Return total Postgres data in the history
***********************************************************************************************************************************/
unsigned int
infoPgDataTotal(const InfoPg *this)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(INFO_PG, this);

        FUNCTION_DEBUG_ASSERT(this != NULL);
    FUNCTION_DEBUG_END();

    FUNCTION_DEBUG_RESULT(UINT, lstSize(this->history));
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
