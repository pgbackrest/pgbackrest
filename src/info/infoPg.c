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
    List *history;                                                  // A list of InfoPgData
    unsigned int historyCurrent;                                    // Index of the current history item
};

OBJECT_DEFINE_FREE(INFO_PG);

/***********************************************************************************************************************************
Internal constructor
***********************************************************************************************************************************/
static InfoPg *
infoPgNewInternal(void)
{
    FUNCTION_LOG_VOID(logLevelTrace);

    InfoPg *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("InfoPg")
    {
        // Create object
        this = memNew(sizeof(InfoPg));
        this->memContext = MEM_CONTEXT_NEW();
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_LOG_RETURN(INFO_PG, this);
}

/***********************************************************************************************************************************
Create new object
***********************************************************************************************************************************/
InfoPg *
infoPgNew(CipherType cipherType, const String *cipherPassSub)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(ENUM, cipherType);
        FUNCTION_TEST_PARAM(STRING, cipherPassSub);
    FUNCTION_LOG_END();

    InfoPg *this = infoPgNewInternal();

    this->history = lstNew(sizeof(InfoPgData));
    this->info = infoNew(cipherType, cipherPassSub);

    FUNCTION_LOG_RETURN(INFO_PG, this);
}

/***********************************************************************************************************************************
Create new object and load contents from a file
***********************************************************************************************************************************/
typedef struct InfoPgLoadData
{
    MemContext *memContext;                                         // Mem context to use for storing data in this structure
    void (*callbackFunction)(                                       // Callback function for child object
        InfoCallbackType type, void *data, const String *section, const String *key, const String *value);
    void *callbackData;                                             // Callback data for child object
    InfoPgType type;                                                // Type of info file being loaded
    unsigned int currentId;                                         // Current database id
    List *history;                                                  // Database history list
    unsigned int historyCurrent;                                    // Current id in history list
} InfoPgLoadData;

static void
infoPgLoadCallback(InfoCallbackType type, void *callbackData, const String *section, const String *key, const String *value)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM_P(VOID, callbackData);
        FUNCTION_TEST_PARAM(STRING, section);
        FUNCTION_TEST_PARAM(STRING, key);
        FUNCTION_TEST_PARAM(STRING, value);
    FUNCTION_TEST_END();

    ASSERT(callbackData != NULL);
    ASSERT(type != infoCallbackTypeValue || section != NULL);
    ASSERT(type != infoCallbackTypeValue || key != NULL);
    ASSERT(type != infoCallbackTypeValue || value != NULL);

    InfoPgLoadData *data = (InfoPgLoadData *)callbackData;

    switch (type)
    {
        // Begin processing
        case infoCallbackTypeBegin:
        {
            MEM_CONTEXT_BEGIN(data->memContext)
            {
                data->history = lstNew(sizeof(InfoPgData));
                data->currentId = 0;
                data->historyCurrent = UINT_MAX;
            }
            MEM_CONTEXT_END();

            // Callback if set
            if (data->callbackFunction != NULL)
                data->callbackFunction(type, data->callbackData, NULL, NULL, NULL);

            break;
        }

        // Reset processing
        case infoCallbackTypeReset:
        {
            lstFree(data->history);

            // Callback if set
            if (data->callbackFunction != NULL)
                data->callbackFunction(type, data->callbackData, NULL, NULL, NULL);

            break;
        }

        // Process values
        case infoCallbackTypeValue:
        {
            // Process db section
            if (strEq(section, INFO_SECTION_DB_STR))
            {
                if (strEq(key, INFO_KEY_DB_ID_STR))
                    data->currentId = jsonToUInt(value);
            }
            // Process db:history section
            else if (strEq(section, INFO_SECTION_DB_HISTORY_STR))
            {
                // Load JSON data into a KeyValue
                const KeyValue *pgDataKv = jsonToKv(value);

                // Get db values that are common to all info files
                InfoPgData infoPgData =
                {
                    .id = cvtZToUInt(strPtr(key)),
                    .version = pgVersionFromStr(varStr(kvGet(pgDataKv, INFO_KEY_DB_VERSION_VAR))),

                    // This is different in archive.info due to a typo that can't be fixed without a format version bump
                    .systemId = varUInt64Force(
                        kvGet(pgDataKv, data->type == infoPgArchive ? INFO_KEY_DB_ID_VAR : INFO_KEY_DB_SYSTEM_ID_VAR)),
                };

                // Get values that are only in backup and manifest files.  These are really vestigial since stanza-create verifies
                // the control and catalog versions so there is no good reason to store them.  However, for backward compatibility
                // we must write them at least, even if we give up reading them.
                if (data->type == infoPgBackup)
                {
                    infoPgData.catalogVersion = varUIntForce(kvGet(pgDataKv, INFO_KEY_DB_CATALOG_VERSION_VAR));
                    infoPgData.controlVersion = varUIntForce(kvGet(pgDataKv, INFO_KEY_DB_CONTROL_VERSION_VAR));
                }
                else if (data->type != infoPgArchive)
                    THROW_FMT(AssertError, "invalid InfoPg type %u", data->type);

                // Using lstAdd because it is more efficient than lstInsert and loading this file is in critical code paths
                lstInsert(data->history, 0, &infoPgData);
            }
            // Callback if set
            else if (data->callbackFunction != NULL)
                data->callbackFunction(infoCallbackTypeValue, data->callbackData, section, key, value);

            break;
        }

        case infoCallbackTypeEnd:
        {
            // History must include at least one item or the file is corrupt
            CHECK(lstSize(data->history) > 0);

            // If the current id was not found then the file is corrupt
            CHECK(data->currentId > 0);

            // Find the current history item
            for (unsigned int historyIdx = 0; historyIdx < lstSize(data->history); historyIdx++)
            {
                if (((InfoPgData *)lstGet(data->history, historyIdx))->id == data->currentId)
                    data->historyCurrent = historyIdx;
            }

            // If the current id did not match the history list then the file is corrupt
            CHECK(data->historyCurrent != UINT_MAX);

            // Callback if set
            if (data->callbackFunction != NULL)
                data->callbackFunction(infoCallbackTypeEnd, data->callbackData, NULL, NULL, NULL);

            break;
        }
    }

    FUNCTION_TEST_RETURN_VOID();
}

InfoPg *
infoPgNewLoad(
    const Storage *storage, const String *fileName, InfoPgType type, CipherType cipherType, const String *cipherPass,
    void (*callbackFunction)(InfoCallbackType type, void *data, const String *section, const String *key, const String *value),
    void *callbackData)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STORAGE, storage);
        FUNCTION_LOG_PARAM(STRING, fileName);
        FUNCTION_LOG_PARAM(ENUM, type);
        FUNCTION_LOG_PARAM(ENUM, cipherType);
        FUNCTION_TEST_PARAM(STRING, cipherPass);
        FUNCTION_LOG_PARAM(FUNCTIONP, callbackFunction);
        FUNCTION_LOG_PARAM_P(VOID, callbackData);
    FUNCTION_LOG_END();

    ASSERT(storage != NULL);
    ASSERT(fileName != NULL);
    ASSERT(cipherType == cipherTypeNone || cipherPass != NULL);
    ASSERT((callbackFunction == NULL && callbackData == NULL) || (callbackFunction != NULL && callbackData != NULL));

    InfoPg *this = infoPgNewInternal();

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Set callback data
        InfoPgLoadData data =
        {
            .memContext = MEM_CONTEXT_TEMP(),
            .callbackFunction = callbackFunction,
            .callbackData = callbackData,
            .type = type,
        };

        // Load info
        MEM_CONTEXT_BEGIN(this->memContext)
        {
            this->info = infoNewLoad(storage, fileName, cipherType, cipherPass, infoPgLoadCallback, &data);
            this->historyCurrent = data.historyCurrent;
            this->history = lstMove(data.history, this->memContext);
        }
        MEM_CONTEXT_END();
    }
    MEM_CONTEXT_TEMP_END();

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
    this->historyCurrent = 0;

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Set the InfoPg object data based on values passed.
***********************************************************************************************************************************/
InfoPg *
infoPgSet(
    InfoPg *this, InfoPgType type, const unsigned int pgVersion, const uint64_t pgSystemId, const uint32_t pgControlVersion,
    const uint32_t pgCatalogVersion)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(INFO_PG, this);
        FUNCTION_LOG_PARAM(ENUM, type);
        FUNCTION_LOG_PARAM(UINT, pgVersion);
        FUNCTION_LOG_PARAM(UINT64, pgSystemId);
        FUNCTION_LOG_PARAM(UINT32, pgControlVersion);
        FUNCTION_TEST_PARAM(UINT32, pgCatalogVersion);
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

        if (type == infoPgBackup)
        {
            infoPgData.catalogVersion = pgCatalogVersion;
            infoPgData.controlVersion = pgControlVersion;
        }
        else if (type != infoPgArchive)
            THROW_FMT(AssertError, "invalid InfoPg type %u", type);

        // Add the pg data to the history list
        infoPgAdd(this, &infoPgData);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(INFO_PG, this);
}

/***********************************************************************************************************************************
Save to file
***********************************************************************************************************************************/
typedef struct InfoPgSaveData
{
    InfoSaveCallback *callbackFunction;                             // Callback function for child object
    void *callbackData;                                             // Callback data for child object
    InfoPgType type;                                                // Type of info file being loaded
    InfoPg *infoPg;                                                 // InfoPg object to be saved
} InfoPgSaveData;

static void
infoPgSaveCallback(void *callbackData, const String *sectionNext, InfoSave *infoSaveData)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM_P(VOID, callbackData);
        FUNCTION_TEST_PARAM(STRING, sectionNext);
        FUNCTION_TEST_PARAM(INFO_SAVE, infoSaveData);
    FUNCTION_TEST_END();

    ASSERT(callbackData != NULL);
    ASSERT(infoSaveData != NULL);

    InfoPgSaveData *data = (InfoPgSaveData *)callbackData;

    if (infoSaveSection(infoSaveData, INFO_SECTION_DB_STR, sectionNext))
    {
        if (data->callbackFunction != NULL)
            data->callbackFunction(data->callbackData, INFO_SECTION_DB_STR, infoSaveData);

        InfoPgData pgData = infoPgDataCurrent(data->infoPg);

        if (data->type == infoPgBackup)
        {
            infoSaveValue(
                infoSaveData, INFO_SECTION_DB_STR, varStr(INFO_KEY_DB_CATALOG_VERSION_VAR), jsonFromUInt(pgData.catalogVersion));
            infoSaveValue(
                infoSaveData, INFO_SECTION_DB_STR, varStr(INFO_KEY_DB_CONTROL_VERSION_VAR), jsonFromUInt(pgData.controlVersion));
        }
        else if (data->type != infoPgArchive)
            THROW_FMT(AssertError, "invalid InfoPg type %u", data->type);

        infoSaveValue(infoSaveData, INFO_SECTION_DB_STR, varStr(INFO_KEY_DB_ID_VAR), jsonFromUInt(pgData.id));
        infoSaveValue(infoSaveData, INFO_SECTION_DB_STR, varStr(INFO_KEY_DB_SYSTEM_ID_VAR), jsonFromUInt64(pgData.systemId));
        infoSaveValue(
            infoSaveData, INFO_SECTION_DB_STR, varStr(INFO_KEY_DB_VERSION_VAR), jsonFromStr(pgVersionToStr(pgData.version)));
    }

    if (infoSaveSection(infoSaveData, INFO_SECTION_DB_HISTORY_STR, sectionNext))
    {
        if (data->callbackFunction != NULL)
            data->callbackFunction(data->callbackData, INFO_SECTION_DB_HISTORY_STR, infoSaveData);

        // Set the db history section in reverse so oldest history is first instead of last to be consistent with load
        for (unsigned int pgDataIdx = infoPgDataTotal(data->infoPg) - 1; (int)pgDataIdx >= 0; pgDataIdx--)
        {
            InfoPgData pgData = infoPgData(data->infoPg, pgDataIdx);

            KeyValue *pgDataKv = kvNew();
            kvPut(pgDataKv, INFO_KEY_DB_VERSION_VAR, VARSTR(pgVersionToStr(pgData.version)));

            if (data->type == infoPgBackup)
            {
                kvPut(pgDataKv, INFO_KEY_DB_CATALOG_VERSION_VAR, VARUINT(pgData.catalogVersion));
                kvPut(pgDataKv, INFO_KEY_DB_CONTROL_VERSION_VAR, VARUINT(pgData.controlVersion));
                kvPut(pgDataKv, INFO_KEY_DB_SYSTEM_ID_VAR, VARUINT64(pgData.systemId));
            }
            else
                kvPut(pgDataKv, INFO_KEY_DB_ID_VAR, VARUINT64(pgData.systemId));

            infoSaveValue(infoSaveData, INFO_SECTION_DB_HISTORY_STR, varStrForce(VARUINT(pgData.id)), jsonFromKv(pgDataKv, 0));
        }
    }

    // Process the callback even if none of the sections above get executed
    if (data->callbackFunction != NULL)
        data->callbackFunction(data->callbackData, sectionNext, infoSaveData);

    FUNCTION_TEST_RETURN_VOID()
}

void
infoPgSave(
    InfoPg *this, const Storage *storage, const String *fileName, InfoPgType type, CipherType cipherType, const String *cipherPass,
    InfoSaveCallback *callbackFunction, void *callbackData)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(INFO_PG, this);
        FUNCTION_LOG_PARAM(STORAGE, storage);
        FUNCTION_LOG_PARAM(STRING, fileName);
        FUNCTION_LOG_PARAM(ENUM, type);
        FUNCTION_LOG_PARAM(ENUM, cipherType);
        FUNCTION_TEST_PARAM(STRING, cipherPass);
        FUNCTION_LOG_PARAM(FUNCTIONP, callbackFunction);
        FUNCTION_LOG_PARAM_P(VOID, callbackData);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(storage != NULL);
    ASSERT(fileName != NULL);
    ASSERT(cipherType == cipherTypeNone || cipherPass != NULL);
    ASSERT((callbackFunction == NULL && callbackData == NULL) || (callbackFunction != NULL && callbackData != NULL));

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Set callback data
        InfoPgSaveData data =
        {
            .callbackFunction = callbackFunction,
            .callbackData = callbackData,
            .type = type,
            .infoPg = this,
        };

        infoSave(infoPgInfo(this), storage, fileName, cipherType, cipherPass, infoPgSaveCallback, &data);
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
