/***********************************************************************************************************************************
Manifest Info Handler
***********************************************************************************************************************************/
#include "build.auto.h"

#include <string.h>

#include "common/debug.h"
#include "common/log.h"
#include "common/type/json.h"
#include "common/type/list.h"
#include "common/type/mcv.h"
#include "info/info.h"
#include "info/infoManifest.h"
#include "postgres/interface.h"
#include "postgres/version.h"
#include "storage/storage.h"

/***********************************************************************************************************************************
Constants
***********************************************************************************************************************************/
VARIANT_STRDEF_EXTERN(INFO_MANIFEST_KEY_BACKUP_ARCHIVE_START_VAR,   INFO_MANIFEST_KEY_BACKUP_ARCHIVE_START);
VARIANT_STRDEF_EXTERN(INFO_MANIFEST_KEY_BACKUP_ARCHIVE_STOP_VAR,    INFO_MANIFEST_KEY_BACKUP_ARCHIVE_STOP);
VARIANT_STRDEF_EXTERN(INFO_MANIFEST_KEY_BACKUP_PRIOR_VAR,           INFO_MANIFEST_KEY_BACKUP_PRIOR);
VARIANT_STRDEF_EXTERN(INFO_MANIFEST_KEY_BACKUP_TIMESTAMP_START_VAR, INFO_MANIFEST_KEY_BACKUP_TIMESTAMP_START);
VARIANT_STRDEF_EXTERN(INFO_MANIFEST_KEY_BACKUP_TIMESTAMP_STOP_VAR,  INFO_MANIFEST_KEY_BACKUP_TIMESTAMP_STOP);
VARIANT_STRDEF_EXTERN(INFO_MANIFEST_KEY_BACKUP_TYPE_VAR,            INFO_MANIFEST_KEY_BACKUP_TYPE);
VARIANT_STRDEF_EXTERN(INFO_MANIFEST_KEY_OPT_ARCHIVE_CHECK_VAR,      INFO_MANIFEST_KEY_OPT_ARCHIVE_CHECK);
VARIANT_STRDEF_EXTERN(INFO_MANIFEST_KEY_OPT_ARCHIVE_COPY_VAR,       INFO_MANIFEST_KEY_OPT_ARCHIVE_COPY);
VARIANT_STRDEF_EXTERN(INFO_MANIFEST_KEY_OPT_BACKUP_STANDBY_VAR,     INFO_MANIFEST_KEY_OPT_BACKUP_STANDBY);
VARIANT_STRDEF_EXTERN(INFO_MANIFEST_KEY_OPT_CHECKSUM_PAGE_VAR,      INFO_MANIFEST_KEY_OPT_CHECKSUM_PAGE);
VARIANT_STRDEF_EXTERN(INFO_MANIFEST_KEY_OPT_COMPRESS_VAR,           INFO_MANIFEST_KEY_OPT_COMPRESS);
VARIANT_STRDEF_EXTERN(INFO_MANIFEST_KEY_OPT_HARDLINK_VAR,           INFO_MANIFEST_KEY_OPT_HARDLINK);
VARIANT_STRDEF_EXTERN(INFO_MANIFEST_KEY_OPT_ONLINE_VAR,             INFO_MANIFEST_KEY_OPT_ONLINE);

STRING_STATIC(INFO_MANIFEST_SECTION_TARGET_FILE_STR,                "target:file");
STRING_STATIC(INFO_MANIFEST_SECTION_TARGET_FILE_DEFAULT_STR,        "target:file:default");

STRING_STATIC(INFO_MANIFEST_SECTION_TARGET_PATH_STR,                "target:path");
STRING_STATIC(INFO_MANIFEST_SECTION_TARGET_PATH_DEFAULT_STR,        "target:path:default");

#define INFO_MANIFEST_KEY_CHECKSUM                                  "checksum"
    VARIANT_STRDEF_STATIC(INFO_MANIFEST_KEY_CHECKSUM_VAR,           INFO_MANIFEST_KEY_CHECKSUM);
#define INFO_MANIFEST_KEY_CHECKSUM_PAGE                             "checksum-page"
    VARIANT_STRDEF_STATIC(INFO_MANIFEST_KEY_CHECKSUM_PAGE_VAR,      INFO_MANIFEST_KEY_CHECKSUM_PAGE);
#define INFO_MANIFEST_KEY_GROUP                                     "group"
    STRING_STATIC(INFO_MANIFEST_KEY_GROUP_STR,                      INFO_MANIFEST_KEY_GROUP);
    VARIANT_STRDEF_STATIC(INFO_MANIFEST_KEY_GROUP_VAR,              INFO_MANIFEST_KEY_GROUP);
#define INFO_MANIFEST_KEY_MASTER                                    "master"
    STRING_STATIC(INFO_MANIFEST_KEY_MASTER_STR,                     INFO_MANIFEST_KEY_MASTER);
    VARIANT_STRDEF_STATIC(INFO_MANIFEST_KEY_MASTER_VAR,             INFO_MANIFEST_KEY_MASTER);
#define INFO_MANIFEST_KEY_MODE                                      "mode"
    STRING_STATIC(INFO_MANIFEST_KEY_MODE_STR,                       INFO_MANIFEST_KEY_MODE);
    VARIANT_STRDEF_STATIC(INFO_MANIFEST_KEY_MODE_VAR,               INFO_MANIFEST_KEY_MODE);
#define INFO_MANIFEST_KEY_SIZE                                      "size"
    VARIANT_STRDEF_STATIC(INFO_MANIFEST_KEY_SIZE_VAR,               INFO_MANIFEST_KEY_SIZE);
#define INFO_MANIFEST_KEY_TIMESTAMP                                 "timestamp"
    VARIANT_STRDEF_STATIC(INFO_MANIFEST_KEY_TIMESTAMP_VAR,          INFO_MANIFEST_KEY_TIMESTAMP);
#define INFO_MANIFEST_KEY_USER                                      "user"
    STRING_STATIC(INFO_MANIFEST_KEY_USER_STR,                       INFO_MANIFEST_KEY_USER);
    VARIANT_STRDEF_STATIC(INFO_MANIFEST_KEY_USER_VAR,               INFO_MANIFEST_KEY_USER);

// STRING_STATIC(INFO_MANIFEST_PATH_PGDATA_STR,                        "pg_data");

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct InfoManifest
{
    MemContext *memContext;                                         // Context that contains the InfoManifest
    unsigned int pgVersion;                                         // PostgreSQL version

    Info *info;                                                     // Base info object

    StringList *ownerList;                                          // List of users/groups

    List *pathList;                                                 // List of paths
    List *fileList;                                                 // List of files
    List *linkList;                                                 // List of links
};

/***********************************************************************************************************************************
Convert the owner to a variant.  The input could be boolean false (meaning there is no owner) or a string (there is an owner).
***********************************************************************************************************************************/
static const Variant *
infoManifestOwnerDefaultGet(InfoManifest *this, const String *ownerDefault)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(INFO_MANIFEST, this);
        FUNCTION_TEST_PARAM(STRING, ownerDefault);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(ownerDefault != NULL);

    FUNCTION_TEST_RETURN(strEq(ownerDefault, FALSE_STR) ? BOOL_FALSE_VAR : varNewStr(jsonToStr(ownerDefault)));
}

/***********************************************************************************************************************************
Add owner to the owner list if it is not there already and return the pointer.  This saves a lot of space.
***********************************************************************************************************************************/
static const String *
infoManifestOwnerCache(InfoManifest *this, const Variant *owner)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(INFO_MANIFEST, this);
        FUNCTION_TEST_PARAM(VARIANT, owner);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(owner != NULL);

    const String *result = NULL;

    if (varType(owner) == varTypeString)
    {
        // Search for the owner in the list
        for (unsigned int ownerIdx = 0; ownerIdx < strLstSize(this->ownerList); ownerIdx++)
        {
            const String *found = strLstGet(this->ownerList, ownerIdx);

            if (strEq(varStr(owner), found))
            {
                result = found;
                break;
            }
        }

        // If not found then add it
        if (result == NULL)
        {
            strLstAdd(this->ownerList, varStr(owner));
            result = strLstGet(this->ownerList, strLstSize(this->ownerList) - 1);
        }
    }

    FUNCTION_TEST_RETURN(result);
}

/***********************************************************************************************************************************
Load from file
***********************************************************************************************************************************/
InfoManifest *
infoManifestNewLoad(const Storage *storage, const String *fileName, CipherType cipherType, const String *cipherPass)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STORAGE, storage);
        FUNCTION_LOG_PARAM(STRING, fileName);
        FUNCTION_LOG_PARAM(ENUM, cipherType);
        FUNCTION_TEST_PARAM(STRING, cipherPass);
    FUNCTION_LOG_END();

    ASSERT(storage != NULL);
    ASSERT(fileName != NULL);
    ASSERT((cipherType == cipherTypeNone && cipherPass == NULL) || (cipherType != cipherTypeNone && cipherPass != NULL));

    InfoManifest *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("InfoManifest")
    {
        // Create object
        this = memNew(sizeof(InfoManifest));
        this->memContext = MEM_CONTEXT_NEW();

        // Create lists
        this->ownerList = strLstNew();
        this->pathList = lstNew(sizeof(InfoManifestPath));
        this->fileList = lstNew(sizeof(InfoManifestFile));
        this->linkList = lstNew(sizeof(InfoManifestLink));

        // Load the manifest
        Ini *iniLocal = NULL;
        this->info = infoNewLoad(storage, fileName, cipherType, cipherPass, &iniLocal);

        MEM_CONTEXT_TEMP_BEGIN()
        {
            // Load path defaults
            // ---------------------------------------------------------------------------------------------------------------------
            const Variant *pathUserDefault = infoManifestOwnerDefaultGet(
                this, iniGet(iniLocal, INFO_MANIFEST_SECTION_TARGET_PATH_DEFAULT_STR, INFO_MANIFEST_KEY_USER_STR));
            const Variant *pathGroupDefault = infoManifestOwnerDefaultGet(
                this, iniGet(iniLocal, INFO_MANIFEST_SECTION_TARGET_PATH_DEFAULT_STR, INFO_MANIFEST_KEY_GROUP_STR));
            const String *pathModeDefault = jsonToStr(
                iniGet(iniLocal, INFO_MANIFEST_SECTION_TARGET_PATH_DEFAULT_STR, INFO_MANIFEST_KEY_MODE_STR));

            // Load path list
            // ---------------------------------------------------------------------------------------------------------------------
            StringList *pathKeyList = iniSectionKeyList(iniLocal, INFO_MANIFEST_SECTION_TARGET_PATH_STR);

            for (unsigned int pathKeyIdx = 0; pathKeyIdx < strLstSize(pathKeyList); pathKeyIdx++)
            {
                memContextSwitch(lstMemContext(this->pathList));
                InfoManifestPath path = {.name = strDup(strLstGet(pathKeyList, pathKeyIdx))};
                memContextSwitch(MEM_CONTEXT_TEMP());

                KeyValue *pathData = varKv(jsonToVar(iniGet(iniLocal, INFO_MANIFEST_SECTION_TARGET_PATH_STR, path.name)));

                path.user = infoManifestOwnerCache(this, kvGetDefault(pathData, INFO_MANIFEST_KEY_USER_VAR, pathUserDefault));
                path.group = infoManifestOwnerCache(this, kvGetDefault(pathData, INFO_MANIFEST_KEY_GROUP_VAR, pathGroupDefault));
                path.mode = cvtZToMode(strPtr(varStr(kvGetDefault(pathData, INFO_MANIFEST_KEY_MODE_VAR, VARSTR(pathModeDefault)))));
                // ??? Set base path?
                // ??? Set db path?

                lstAdd(this->pathList, &path);
            }

            // Load file defaults
            // ---------------------------------------------------------------------------------------------------------------------
            const Variant *fileUserDefault = infoManifestOwnerDefaultGet(
                this, iniGet(iniLocal, INFO_MANIFEST_SECTION_TARGET_FILE_DEFAULT_STR, INFO_MANIFEST_KEY_USER_STR));
            const Variant *fileGroupDefault = infoManifestOwnerDefaultGet(
                this, iniGet(iniLocal, INFO_MANIFEST_SECTION_TARGET_FILE_DEFAULT_STR, INFO_MANIFEST_KEY_GROUP_STR));
            const String *fileModeDefault = jsonToStr(
                iniGet(iniLocal, INFO_MANIFEST_SECTION_TARGET_FILE_DEFAULT_STR, INFO_MANIFEST_KEY_MODE_STR));
            const Variant *fileMasterDefault = jsonToVar(
                iniGet(iniLocal, INFO_MANIFEST_SECTION_TARGET_FILE_DEFAULT_STR, INFO_MANIFEST_KEY_MASTER_STR));

            // Load file list
            // ---------------------------------------------------------------------------------------------------------------------
            StringList *fileKeyList = iniSectionKeyList(iniLocal, INFO_MANIFEST_SECTION_TARGET_FILE_STR);

            for (unsigned int fileKeyIdx = 0; fileKeyIdx < strLstSize(fileKeyList); fileKeyIdx++)
            {
                memContextSwitch(lstMemContext(this->fileList));
                InfoManifestFile file = {.name = strDup(strLstGet(fileKeyList, fileKeyIdx))};
                memContextSwitch(MEM_CONTEXT_TEMP());

                KeyValue *fileData = varKv(jsonToVar(iniGet(iniLocal, INFO_MANIFEST_SECTION_TARGET_FILE_STR, file.name)));

    // const String *checksumPageError;                                    // JSON result when there are checksum errors

                file.user = infoManifestOwnerCache(this, kvGetDefault(fileData, INFO_MANIFEST_KEY_USER_VAR, fileUserDefault));
                file.group = infoManifestOwnerCache(this, kvGetDefault(fileData, INFO_MANIFEST_KEY_GROUP_VAR, fileGroupDefault));
                file.mode = cvtZToMode(strPtr(varStr(kvGetDefault(fileData, INFO_MANIFEST_KEY_MODE_VAR, VARSTR(fileModeDefault)))));
                file.master = varBool(kvGetDefault(fileData, INFO_MANIFEST_KEY_MASTER_VAR, fileMasterDefault));
                file.checksumPage = varBool(kvGetDefault(fileData, INFO_MANIFEST_KEY_CHECKSUM_PAGE_VAR, BOOL_FALSE_VAR));
                file.size = varUInt64(kvGet(fileData, INFO_MANIFEST_KEY_SIZE_VAR));
                file.sizeRepo = varUInt64(kvGetDefault(fileData, INFO_MANIFEST_KEY_SIZE_VAR, VARUINT64(file.size)));
                file.timestamp = (time_t)varUInt64(kvGet(fileData, INFO_MANIFEST_KEY_TIMESTAMP_VAR));

                if (file.size == 0)
                    memcpy(file.checksumSha1, HASH_TYPE_SHA1_ZERO, HASH_TYPE_SHA1_SIZE_HEX + 1);
                else
                {
                    memcpy(
                        file.checksumSha1, strPtr(varStr(kvGet(fileData, INFO_MANIFEST_KEY_CHECKSUM_VAR))),
                        HASH_TYPE_SHA1_SIZE_HEX + 1);
                }

                lstAdd(this->fileList, &file);
            }
        }
        MEM_CONTEXT_TEMP_END();
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_LOG_RETURN(INFO_MANIFEST, this);
}

/***********************************************************************************************************************************
Save to file
***********************************************************************************************************************************/
// Convert the owner MCV to a default.  If the input is NULL boolean false should be return, else the owner string.
static const Variant *
infoManifestOwnerGet(const String *ownerDefault)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, ownerDefault);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RETURN(ownerDefault == NULL ? BOOL_FALSE_VAR : varNewStr(ownerDefault));
}

void
infoManifestSave(
    InfoManifest *this, const Storage *storage, const String *fileName, CipherType cipherType, const String *cipherPass)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(INFO_MANIFEST, this);
        FUNCTION_LOG_PARAM(STORAGE, storage);
        FUNCTION_LOG_PARAM(STRING, fileName);
        FUNCTION_LOG_PARAM(ENUM, cipherType);
        FUNCTION_TEST_PARAM(STRING, cipherPass);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(storage != NULL);
    ASSERT(fileName != NULL);
    ASSERT((cipherType == cipherTypeNone && cipherPass == NULL) || (cipherType != cipherTypeNone && cipherPass != NULL));

    MEM_CONTEXT_TEMP_BEGIN()
    {
        Ini *ini = iniNew();

        // Save default path values
        // -------------------------------------------------------------------------------------------------------------------------
        MostCommonValue *userMcv = mcvNew(varTypeString);
        MostCommonValue *groupMcv = mcvNew(varTypeString);
        MostCommonValue *modeMcv = mcvNew(varTypeUInt);

        for (unsigned int pathIdx = 0; pathIdx < lstSize(this->pathList); pathIdx++)
        {
            InfoManifestPath *path = lstGet(this->pathList, pathIdx);

            mcvUpdate(userMcv, VARSTR(path->user));
            mcvUpdate(groupMcv, VARSTR(path->group));
            mcvUpdate(modeMcv, VARUINT(path->mode));
        }

        const Variant *userDefault = infoManifestOwnerGet(varStr(mcvResult(userMcv)));
        const Variant *groupDefault = infoManifestOwnerGet(varStr(mcvResult(groupMcv)));
        const Variant *modeDefault = mcvResult(modeMcv);

        iniSet(ini, INFO_MANIFEST_SECTION_TARGET_PATH_DEFAULT_STR, INFO_MANIFEST_KEY_USER_STR, jsonFromVar(userDefault, 0));
        iniSet(ini, INFO_MANIFEST_SECTION_TARGET_PATH_DEFAULT_STR, INFO_MANIFEST_KEY_GROUP_STR, jsonFromVar(groupDefault, 0));
        iniSet(
            ini, INFO_MANIFEST_SECTION_TARGET_PATH_DEFAULT_STR, INFO_MANIFEST_KEY_MODE_STR,
            jsonFromStr(strNewFmt("%04o", varUInt(modeDefault))));

        // Save path list
        // -------------------------------------------------------------------------------------------------------------------------
        for (unsigned int pathIdx = 0; pathIdx < lstSize(this->pathList); pathIdx++)
        {
            InfoManifestPath *path = lstGet(this->pathList, pathIdx);
            KeyValue *pathData = kvNew();

            if (!varEq(infoManifestOwnerGet(path->user), userDefault))
                kvPut(pathData, INFO_MANIFEST_KEY_USER_VAR, infoManifestOwnerGet(path->user));

            if (!varEq(infoManifestOwnerGet(path->group), groupDefault))
                kvPut(pathData, INFO_MANIFEST_KEY_GROUP_VAR, infoManifestOwnerGet(path->group));

            if (!varEq(VARUINT(path->mode), modeDefault))
                kvPut(pathData, INFO_MANIFEST_KEY_MODE_VAR, VARSTR(strNewFmt("%04o", path->mode)));

            iniSet(ini, INFO_MANIFEST_SECTION_TARGET_PATH_STR, path->name, jsonFromKv(pathData, 0));
        }

        infoSave(this->info, ini, storage, fileName, cipherType, cipherPass);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}
