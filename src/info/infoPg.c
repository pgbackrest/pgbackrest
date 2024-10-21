/***********************************************************************************************************************************
PostgreSQL Info Handler
***********************************************************************************************************************************/
#include "build.auto.h"

#include <limits.h>
#include <stdarg.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/debug.h"
#include "common/ini.h"
#include "common/log.h"
#include "common/type/json.h"
#include "common/type/list.h"
#include "common/type/object.h"
#include "info/info.h"
#include "info/infoPg.h"
#include "postgres/interface.h"
#include "postgres/version.h"
#include "storage/helper.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct InfoPg
{
    InfoPgPub pub;                                                  // Publicly accessible variables
    MemContext *memContext;                                         // Mem context
    InfoPgType type;                                                // Type of info file being loaded
    unsigned int historyCurrent;                                    // Index of the current history item
};

/***********************************************************************************************************************************
Internal constructor
***********************************************************************************************************************************/
static InfoPg *
infoPgNewInternal(const InfoPgType type)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING_ID, type);
    FUNCTION_TEST_END();

    FUNCTION_AUDIT_HELPER();

    InfoPg *this = OBJ_NEW_ALLOC();

    *this = (InfoPg)
    {
        .pub =
        {
            .history = lstNewP(sizeof(InfoPgData)),
        },
        .memContext = memContextCurrent(),
        .type = type,
    };

    FUNCTION_TEST_RETURN(INFO_PG, this);
}

/**********************************************************************************************************************************/
FN_EXTERN InfoPg *
infoPgNew(const InfoPgType type, const String *const cipherPassSub)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STRING_ID, type);
        FUNCTION_TEST_PARAM(STRING, cipherPassSub);
    FUNCTION_LOG_END();

    InfoPg *this;

    OBJ_NEW_BASE_BEGIN(InfoPg, .childQty = MEM_CONTEXT_QTY_MAX)
    {
        this = infoPgNewInternal(type);
        this->pub.info = infoNew(cipherPassSub);
    }
    OBJ_NEW_END();

    FUNCTION_LOG_RETURN(INFO_PG, this);
}

/**********************************************************************************************************************************/
#define INFO_SECTION_DB                                                     "db"
#define INFO_SECTION_DB_HISTORY                                             "db:history"

#define INFO_KEY_DB_CATALOG_VERSION                                         "db-catalog-version"
#define INFO_KEY_DB_CONTROL_VERSION                                         "db-control-version"
#define INFO_KEY_DB_SYSTEM_ID                                               "db-system-id"
#define INFO_KEY_DB_VERSION                                                 "db-version"

typedef struct InfoPgLoadData
{
    InfoLoadNewCallback *callbackFunction;                          // Callback function for child object
    void *callbackData;                                             // Callback data for child object
    InfoPg *infoPg;                                                 // Pg info
    unsigned int currentId;                                         // Current database id
} InfoPgLoadData;

static void
infoPgLoadCallback(void *const data, const String *const section, const String *const key, const String *const value)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM_P(VOID, data);
        FUNCTION_TEST_PARAM(STRING, section);
        FUNCTION_TEST_PARAM(STRING, key);
        FUNCTION_TEST_PARAM(STRING, value);
    FUNCTION_TEST_END();

    FUNCTION_AUDIT_CALLBACK();

    ASSERT(data != NULL);
    ASSERT(section != NULL);
    ASSERT(key != NULL);
    ASSERT(value != NULL);

    InfoPgLoadData *const loadData = data;

    // Process db section
    if (strEqZ(section, INFO_SECTION_DB))
    {
        if (strEqZ(key, INFO_KEY_DB_ID))
            loadData->currentId = varUIntForce(jsonToVar(value));
    }
    // Process db:history section
    else if (strEqZ(section, INFO_SECTION_DB_HISTORY))
    {
        InfoPgData infoPgData = {.id = cvtZToUInt(strZ(key))};

        JsonRead *const json = jsonReadNew(value);
        jsonReadObjectBegin(json);

        // Catalog version
        if (loadData->infoPg->type == infoPgBackup)
        {
            infoPgData.catalogVersion = jsonReadUInt(jsonReadKeyRequireZ(json, INFO_KEY_DB_CATALOG_VERSION));
            infoPgData.controlVersion = jsonReadUInt(jsonReadKeyRequireZ(json, INFO_KEY_DB_CONTROL_VERSION));
        }

        // System id
        infoPgData.systemId = jsonReadUInt64(
            jsonReadKeyRequireZ(json, loadData->infoPg->type == infoPgArchive ? INFO_KEY_DB_ID : INFO_KEY_DB_SYSTEM_ID));

        // PostgreSQL version
        infoPgData.version = pgVersionFromStr(jsonReadStr(jsonReadKeyRequireZ(json, INFO_KEY_DB_VERSION)));

        // Insert at beginning of list so the history is reverse ordered
        lstInsert(loadData->infoPg->pub.history, 0, &infoPgData);
    }
    // Callback if set
    else if (loadData->callbackFunction != NULL)
        loadData->callbackFunction(loadData->callbackData, section, key, value);

    FUNCTION_TEST_RETURN_VOID();
}

FN_EXTERN InfoPg *
infoPgNewLoad(IoRead *const read, const InfoPgType type, InfoLoadNewCallback *const callbackFunction, void *const callbackData)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(IO_READ, read);
        FUNCTION_LOG_PARAM(STRING_ID, type);
        FUNCTION_LOG_PARAM(FUNCTIONP, callbackFunction);
        FUNCTION_LOG_PARAM_P(VOID, callbackData);
    FUNCTION_LOG_END();

    ASSERT(read != NULL);
    ASSERT(type == infoPgBackup || type == infoPgArchive);
    ASSERT((callbackFunction == NULL && callbackData == NULL) || (callbackFunction != NULL && callbackData != NULL));

    InfoPg *this;

    OBJ_NEW_BASE_BEGIN(InfoPg, .childQty = MEM_CONTEXT_QTY_MAX)
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

        this->pub.info = infoNewLoad(read, infoPgLoadCallback, &loadData);

        CHECK(FormatError, !lstEmpty(this->pub.history), "history is missing");
        CHECK(FormatError, loadData.currentId > 0, "current id is missing");

        // Find the current history item
        for (unsigned int historyIdx = 0; historyIdx < lstSize(this->pub.history); historyIdx++)
        {
            if (((InfoPgData *)lstGet(this->pub.history, historyIdx))->id == loadData.currentId)
                this->historyCurrent = historyIdx;
        }

        CHECK(FormatError, this->historyCurrent != UINT_MAX, "unable to find current id in history");
    }
    OBJ_NEW_END();

    FUNCTION_LOG_RETURN(INFO_PG, this);
}

/**********************************************************************************************************************************/
FN_EXTERN void
infoPgAdd(InfoPg *const this, const InfoPgData *const infoPgData)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(INFO_PG, this);
        FUNCTION_LOG_PARAM_P(INFO_PG_DATA, infoPgData);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(infoPgData != NULL);

    lstInsert(this->pub.history, 0, infoPgData);
    this->historyCurrent = 0;

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
FN_EXTERN InfoPg *
infoPgSet(
    InfoPg *const this, const InfoPgType type, const unsigned int pgVersion, const uint64_t pgSystemId,
    const unsigned int pgCatalogVersion)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(INFO_PG, this);
        FUNCTION_LOG_PARAM(STRING_ID, type);
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
        };

        // Catalog/control version required for backup info to preserve the repo format
        if (this->type == infoPgBackup)
        {
            infoPgData.catalogVersion = pgCatalogVersion;
            infoPgData.controlVersion = pgControlVersion(pgVersion);
        }

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
infoPgSaveCallback(void *const data, const String *const sectionNext, InfoSave *const infoSaveData)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM_P(VOID, data);
        FUNCTION_TEST_PARAM(STRING, sectionNext);
        FUNCTION_TEST_PARAM(INFO_SAVE, infoSaveData);
    FUNCTION_TEST_END();

    FUNCTION_AUDIT_CALLBACK();

    ASSERT(data != NULL);
    ASSERT(infoSaveData != NULL);

    InfoPgSaveData *const saveData = (InfoPgSaveData *)data;

    if (infoSaveSection(infoSaveData, INFO_SECTION_DB, sectionNext))
    {
        if (saveData->callbackFunction != NULL)
            saveData->callbackFunction(saveData->callbackData, STRDEF(INFO_SECTION_DB), infoSaveData);

        const InfoPgData pgData = infoPgDataCurrent(saveData->infoPg);

        // These need to be saved because older pgBackRest versions expect them
        if (saveData->infoPg->type == infoPgBackup)
        {
            infoSaveValue(infoSaveData, INFO_SECTION_DB, INFO_KEY_DB_CATALOG_VERSION, jsonFromVar(VARUINT(pgData.catalogVersion)));
            infoSaveValue(infoSaveData, INFO_SECTION_DB, INFO_KEY_DB_CONTROL_VERSION, jsonFromVar(VARUINT(pgData.controlVersion)));
        }

        infoSaveValue(infoSaveData, INFO_SECTION_DB, INFO_KEY_DB_ID, jsonFromVar(VARUINT(pgData.id)));
        infoSaveValue(infoSaveData, INFO_SECTION_DB, INFO_KEY_DB_SYSTEM_ID, jsonFromVar(VARUINT64(pgData.systemId)));
        infoSaveValue(infoSaveData, INFO_SECTION_DB, INFO_KEY_DB_VERSION, jsonFromVar(VARSTR(pgVersionToStr(pgData.version))));
    }

    if (infoSaveSection(infoSaveData, INFO_SECTION_DB_HISTORY, sectionNext))
    {
        if (saveData->callbackFunction != NULL)
            saveData->callbackFunction(saveData->callbackData, STRDEF(INFO_SECTION_DB_HISTORY), infoSaveData);

        // Set the db history section in reverse so oldest history is first instead of last to be consistent with load
        for (unsigned int pgDataIdx = infoPgDataTotal(saveData->infoPg) - 1; (int)pgDataIdx >= 0; pgDataIdx--)
        {
            const InfoPgData pgData = infoPgData(saveData->infoPg, pgDataIdx);
            JsonWrite *const json = jsonWriteObjectBegin(jsonWriteNewP());

            // These need to be saved because older pgBackRest versions expect them
            if (saveData->infoPg->type == infoPgBackup)
            {
                jsonWriteUInt(jsonWriteKeyZ(json, INFO_KEY_DB_CATALOG_VERSION), pgData.catalogVersion);
                jsonWriteUInt(jsonWriteKeyZ(json, INFO_KEY_DB_CONTROL_VERSION), pgData.controlVersion);
            }

            if (saveData->infoPg->type == infoPgArchive)
                jsonWriteUInt64(jsonWriteKeyZ(json, INFO_KEY_DB_ID), pgData.systemId);

            if (saveData->infoPg->type == infoPgBackup)
                jsonWriteUInt64(jsonWriteKeyZ(json, INFO_KEY_DB_SYSTEM_ID), pgData.systemId);

            jsonWriteStr(jsonWriteKeyZ(json, INFO_KEY_DB_VERSION), pgVersionToStr(pgData.version));

            infoSaveValue(
                infoSaveData, INFO_SECTION_DB_HISTORY, strZ(varStrForce(VARUINT(pgData.id))),
                jsonWriteResult(jsonWriteObjectEnd(json)));
        }
    }

    // Process the callback even if none of the sections above get executed
    if (saveData->callbackFunction != NULL)
        saveData->callbackFunction(saveData->callbackData, sectionNext, infoSaveData);

    FUNCTION_TEST_RETURN_VOID();
}

FN_EXTERN void
infoPgSave(InfoPg *const this, IoWrite *const write, InfoSaveCallback *const callbackFunction, void *const callbackData)
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
FN_EXTERN String *
infoPgArchiveId(const InfoPg *const this, const unsigned int pgDataIdx)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(INFO_PG, this);
        FUNCTION_LOG_PARAM(UINT, pgDataIdx);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    const InfoPgData pgData = infoPgData(this, pgDataIdx);
    String *const version = pgVersionToStr(pgData.version);
    String *const result = strNewFmt("%s-%u", strZ(version), pgData.id);

    strFree(version);

    FUNCTION_LOG_RETURN(STRING, result);
}

/**********************************************************************************************************************************/
FN_EXTERN InfoPgData
infoPgData(const InfoPg *const this, const unsigned int pgDataIdx)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(INFO_PG, this);
        FUNCTION_LOG_PARAM(UINT, pgDataIdx);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    FUNCTION_LOG_RETURN(INFO_PG_DATA, *(InfoPgData *)lstGet(this->pub.history, pgDataIdx));
}

/**********************************************************************************************************************************/
FN_EXTERN InfoPgData
infoPgDataCurrent(const InfoPg *const this)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(INFO_PG, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    FUNCTION_LOG_RETURN(INFO_PG_DATA, infoPgData(this, infoPgDataCurrentId(this)));
}

/**********************************************************************************************************************************/
FN_EXTERN unsigned int
infoPgDataCurrentId(const InfoPg *const this)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(INFO_PG, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    FUNCTION_LOG_RETURN(UINT, this->historyCurrent);
}

/**********************************************************************************************************************************/
FN_EXTERN unsigned int
infoPgCurrentDataId(const InfoPg *const this)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(INFO_PG, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    InfoPgData currentData = infoPgDataCurrent(this);

    FUNCTION_LOG_RETURN(UINT, currentData.id);
}

/**********************************************************************************************************************************/
FN_EXTERN void
infoPgDataToLog(const InfoPgData *const this, StringStatic *const debugLog)
{
    strStcFmt(
        debugLog, "{id: %u, version: %u, systemId: %" PRIu64 ", catalogVersion: %u}", this->id, this->version, this->systemId,
        this->catalogVersion);
}
