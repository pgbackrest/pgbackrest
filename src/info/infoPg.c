/***********************************************************************************************************************************
PostgreSQL Info Handler
***********************************************************************************************************************************/
#include "build.auto.h"

#include <limits.h>
#include <stdarg.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "common/debug.h"
#include "common/ini.h"
#include "common/log.h"
#include "common/memContext.h"
#include "common/type/json.h"
#include "common/type/list.h"
#include "common/type/object.h"
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

STRING_STATIC(INFO_KEY_DB_ID_STR,                                           INFO_KEY_DB_ID);
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
    InfoPgType type;                                                // Type of info file being loaded
    List *history;                                                  // A list of InfoPgData
    unsigned int historyCurrent;                                    // Index of the current history item
};

/***********************************************************************************************************************************
Internal constructor
***********************************************************************************************************************************/
static InfoPg *
infoPgNewInternal(InfoPgType type)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, type);
    FUNCTION_TEST_END();

    InfoPg *this = memNew(sizeof(InfoPg));

    *this = (InfoPg)
    {
        .memContext = memContextCurrent(),
        .type = type,
        .history = lstNewP(sizeof(InfoPgData)),
    };

    FUNCTION_TEST_RETURN(this);
}

/**********************************************************************************************************************************/
InfoPg *
infoPgNew(InfoPgType type, const String *cipherPassSub)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(ENUM, type);
        FUNCTION_TEST_PARAM(STRING, cipherPassSub);
    FUNCTION_LOG_END();

    InfoPg *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("InfoPg")
    {
        this = infoPgNewInternal(type);
        this->info = infoNew(cipherPassSub);
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_LOG_RETURN(INFO_PG, this);
}

/**********************************************************************************************************************************/
typedef struct InfoPgLoadData
{
    InfoLoadNewCallback *callbackFunction;                          // Callback function for child object
    void *callbackData;                                             // Callback data for child object
    InfoPg *infoPg;                                                 // Pg info
    unsigned int currentId;                                         // Current database id
} InfoPgLoadData;

static void
infoPgLoadCallback(void *data, const String *section, const String *key, const Variant *value)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM_P(VOID, data);
        FUNCTION_TEST_PARAM(STRING, section);
        FUNCTION_TEST_PARAM(STRING, key);
        FUNCTION_TEST_PARAM(VARIANT, value);
    FUNCTION_TEST_END();

    ASSERT(data != NULL);
    ASSERT(section != NULL);
    ASSERT(key != NULL);
    ASSERT(value != NULL);

    InfoPgLoadData *loadData = (InfoPgLoadData *)data;

    // Process db section
    if (strEq(section, INFO_SECTION_DB_STR))
    {
        if (strEq(key, INFO_KEY_DB_ID_STR))
            loadData->currentId = varUIntForce(value);
    }
    // Process db:history section
    else if (strEq(section, INFO_SECTION_DB_HISTORY_STR))
    {
        // Get db values that are common to all info files
        const KeyValue *pgDataKv = varKv(value);

        InfoPgData infoPgData =
        {
            .id = cvtZToUInt(strZ(key)),
            .version = pgVersionFromStr(varStr(kvGet(pgDataKv, INFO_KEY_DB_VERSION_VAR))),
            .catalogVersion =
                loadData->infoPg->type == infoPgBackup ? varUIntForce(kvGet(pgDataKv, INFO_KEY_DB_CATALOG_VERSION_VAR)) : 0,

            // This is different in archive.info due to a typo that can't be fixed without a format version bump
            .systemId = varUInt64Force(
                kvGet(pgDataKv, loadData->infoPg->type == infoPgArchive ? INFO_KEY_DB_ID_VAR : INFO_KEY_DB_SYSTEM_ID_VAR)),
        };

        // Insert at beginning of list so the history is reverse ordered
        lstInsert(loadData->infoPg->history, 0, &infoPgData);
    }
    // Callback if set
    else if (loadData->callbackFunction != NULL)
        loadData->callbackFunction(loadData->callbackData, section, key, value);

    FUNCTION_TEST_RETURN_VOID();
}

InfoPg *
infoPgNewLoad(IoRead *read, InfoPgType type, InfoLoadNewCallback *callbackFunction, void *callbackData)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(IO_READ, read);
        FUNCTION_LOG_PARAM(ENUM, type);
        FUNCTION_LOG_PARAM(FUNCTIONP, callbackFunction);
        FUNCTION_LOG_PARAM_P(VOID, callbackData);
    FUNCTION_LOG_END();

    ASSERT(read != NULL);
    ASSERT(type == infoPgBackup || type == infoPgArchive);
    ASSERT((callbackFunction == NULL && callbackData == NULL) || (callbackFunction != NULL && callbackData != NULL));

    InfoPg *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("InfoPg")
    {
        this = infoPgNewInternal(type);

        // Set historyCurrent to UINT_MAX so we can detect if it was loaded correctly
        this->historyCurrent = UINT_MAX;

        // Load
        InfoPgLoadData loadData =
        {
            .callbackFunction = callbackFunction,
            .callbackData = callbackData,
            .infoPg = this,
        };

        this->info = infoNewLoad(read, infoPgLoadCallback, &loadData);

        // History must include at least one item or the file is corrupt
        CHECK(lstSize(this->history) > 0);

        // If the current id was not found then the file is corrupt
        CHECK(loadData.currentId > 0);

        // Find the current history item
        for (unsigned int historyIdx = 0; historyIdx < lstSize(this->history); historyIdx++)
        {
            if (((InfoPgData *)lstGet(this->history, historyIdx))->id == loadData.currentId)
                this->historyCurrent = historyIdx;
        }

        // If the current id did not match the history list then the file is corrupt
        CHECK(this->historyCurrent != UINT_MAX);
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_LOG_RETURN(INFO_PG, this);
}

/***********************************************************************************************************************************
Add Postgres data to the history list at position 0 to ensure the latest history is always first in the list
***********************************************************************************************************************************/
static void
infoPgAdd(InfoPg *this, const InfoPgData *infoPgData)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(INFO_PG, this);
        FUNCTION_LOG_PARAM_P(INFO_PG_DATA, infoPgData);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(infoPgData != NULL);

    lstInsert(this->history, 0, infoPgData);
    this->historyCurrent = 0;

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
InfoPg *
infoPgSet(
    InfoPg *this, InfoPgType type, const unsigned int pgVersion, const uint64_t pgSystemId, const unsigned int pgCatalogVersion)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(INFO_PG, this);
        FUNCTION_LOG_PARAM(ENUM, type);
        FUNCTION_LOG_PARAM(UINT, pgVersion);
        FUNCTION_LOG_PARAM(UINT64, pgSystemId);
        FUNCTION_LOG_PARAM(UINT, pgCatalogVersion);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        unsigned int pgDataId = 1;

        // If there is some history, then get the historyId of the most current and increment it
        if (infoPgDataTotal(this) > 0)
            pgDataId = infoPgCurrentDataId(this) + 1;

        // Set db values that are common to all info files
        InfoPgData infoPgData =
        {
            .id = pgDataId,
            .version = pgVersion,

            // This is different in archive.info due to a typo that can't be fixed without a format version bump
            .systemId = pgSystemId,

            // Catalog version is only required for backup info to preserve the repo format
            .catalogVersion = this->type == infoPgBackup ? pgCatalogVersion : 0,
        };

        // Add the pg data to the history list
        infoPgAdd(this, &infoPgData);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(INFO_PG, this);
}

/**********************************************************************************************************************************/
typedef struct InfoPgSaveData
{
    InfoSaveCallback *callbackFunction;                             // Callback function for child object
    void *callbackData;                                             // Callback data for child object
    InfoPgType type;                                                // Type of info file being loaded
    InfoPg *infoPg;                                                 // InfoPg object to be saved
} InfoPgSaveData;

static void
infoPgSaveCallback(void *data, const String *sectionNext, InfoSave *infoSaveData)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM_P(VOID, data);
        FUNCTION_TEST_PARAM(STRING, sectionNext);
        FUNCTION_TEST_PARAM(INFO_SAVE, infoSaveData);
    FUNCTION_TEST_END();

    ASSERT(data != NULL);
    ASSERT(infoSaveData != NULL);

    InfoPgSaveData *saveData = (InfoPgSaveData *)data;

    if (infoSaveSection(infoSaveData, INFO_SECTION_DB_STR, sectionNext))
    {
        if (saveData->callbackFunction != NULL)
            saveData->callbackFunction(saveData->callbackData, INFO_SECTION_DB_STR, infoSaveData);

        InfoPgData pgData = infoPgDataCurrent(saveData->infoPg);

        // These need to be saved because older pgBackRest versions expect them
        if (saveData->infoPg->type == infoPgBackup)
        {
            infoSaveValue(
                infoSaveData, INFO_SECTION_DB_STR, varStr(INFO_KEY_DB_CATALOG_VERSION_VAR), jsonFromUInt(pgData.catalogVersion));
            infoSaveValue(
                infoSaveData, INFO_SECTION_DB_STR, varStr(INFO_KEY_DB_CONTROL_VERSION_VAR),
                jsonFromUInt(pgControlVersion(pgData.version)));
        }

        infoSaveValue(infoSaveData, INFO_SECTION_DB_STR, varStr(INFO_KEY_DB_ID_VAR), jsonFromUInt(pgData.id));
        infoSaveValue(infoSaveData, INFO_SECTION_DB_STR, varStr(INFO_KEY_DB_SYSTEM_ID_VAR), jsonFromUInt64(pgData.systemId));
        infoSaveValue(
            infoSaveData, INFO_SECTION_DB_STR, varStr(INFO_KEY_DB_VERSION_VAR), jsonFromStr(pgVersionToStr(pgData.version)));
    }

    if (infoSaveSection(infoSaveData, INFO_SECTION_DB_HISTORY_STR, sectionNext))
    {
        if (saveData->callbackFunction != NULL)
            saveData->callbackFunction(saveData->callbackData, INFO_SECTION_DB_HISTORY_STR, infoSaveData);

        // Set the db history section in reverse so oldest history is first instead of last to be consistent with load
        for (unsigned int pgDataIdx = infoPgDataTotal(saveData->infoPg) - 1; (int)pgDataIdx >= 0; pgDataIdx--)
        {
            InfoPgData pgData = infoPgData(saveData->infoPg, pgDataIdx);

            KeyValue *pgDataKv = kvNew();
            kvPut(pgDataKv, INFO_KEY_DB_VERSION_VAR, VARSTR(pgVersionToStr(pgData.version)));

            if (saveData->infoPg->type == infoPgBackup)
            {
                kvPut(pgDataKv, INFO_KEY_DB_SYSTEM_ID_VAR, VARUINT64(pgData.systemId));

                // These need to be saved because older pgBackRest versions expect them
                kvPut(pgDataKv, INFO_KEY_DB_CATALOG_VERSION_VAR, VARUINT(pgData.catalogVersion));
                kvPut(pgDataKv, INFO_KEY_DB_CONTROL_VERSION_VAR, VARUINT(pgControlVersion(pgData.version)));
            }
            else
                kvPut(pgDataKv, INFO_KEY_DB_ID_VAR, VARUINT64(pgData.systemId));

            infoSaveValue(infoSaveData, INFO_SECTION_DB_HISTORY_STR, varStrForce(VARUINT(pgData.id)), jsonFromKv(pgDataKv));
        }
    }

    // Process the callback even if none of the sections above get executed
    if (saveData->callbackFunction != NULL)
        saveData->callbackFunction(saveData->callbackData, sectionNext, infoSaveData);

    FUNCTION_TEST_RETURN_VOID()
}

void
infoPgSave(InfoPg *this, IoWrite *write, InfoSaveCallback *callbackFunction, void *callbackData)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(INFO_PG, this);
        FUNCTION_LOG_PARAM(IO_WRITE, write);
        FUNCTION_LOG_PARAM(FUNCTIONP, callbackFunction);
        FUNCTION_LOG_PARAM_P(VOID, callbackData);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(write != NULL);
    ASSERT((callbackFunction == NULL && callbackData == NULL) || (callbackFunction != NULL && callbackData != NULL));

    MEM_CONTEXT_TEMP_BEGIN()
    {
        InfoPgSaveData saveData =
        {
            .callbackFunction = callbackFunction,
            .callbackData = callbackData,
            .infoPg = this,
        };

        infoSave(infoPgInfo(this), write, infoPgSaveCallback, &saveData);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
String *
infoPgArchiveId(const InfoPg *this, unsigned int pgDataIdx)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(INFO_PG, this);
        FUNCTION_LOG_PARAM(UINT, pgDataIdx);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    InfoPgData pgData = infoPgData(this, pgDataIdx);

    FUNCTION_LOG_RETURN(STRING, strNewFmt("%s-%u", strZ(pgVersionToStr(pgData.version)), pgData.id));
}

/**********************************************************************************************************************************/
const String *
infoPgCipherPass(const InfoPg *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(INFO_PG, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(infoCipherPass(this->info));
}

/**********************************************************************************************************************************/
InfoPgData
infoPgData(const InfoPg *this, unsigned int pgDataIdx)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(INFO_PG, this);
        FUNCTION_LOG_PARAM(UINT, pgDataIdx);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    FUNCTION_LOG_RETURN(INFO_PG_DATA, *(InfoPgData *)lstGet(this->history, pgDataIdx));
}

/**********************************************************************************************************************************/
InfoPgData
infoPgDataCurrent(const InfoPg *this)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(INFO_PG, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    FUNCTION_LOG_RETURN(INFO_PG_DATA, infoPgData(this, infoPgDataCurrentId(this)));
}

/**********************************************************************************************************************************/
unsigned int
infoPgDataCurrentId(const InfoPg *this)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(INFO_PG, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    FUNCTION_LOG_RETURN(UINT, this->historyCurrent);
}

/**********************************************************************************************************************************/
Info *
infoPgInfo(const InfoPg *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(INFO_PG, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->info);
}

/**********************************************************************************************************************************/
unsigned int
infoPgDataTotal(const InfoPg *this)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(INFO_PG, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    FUNCTION_LOG_RETURN(UINT, lstSize(this->history));
}

/**********************************************************************************************************************************/
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

/**********************************************************************************************************************************/
String *
infoPgDataToLog(const InfoPgData *this)
{
    return strNewFmt("{id: %u, version: %u, systemId: %" PRIu64 "}", this->id, this->version, this->systemId);
}
