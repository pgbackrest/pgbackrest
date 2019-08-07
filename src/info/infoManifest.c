/***********************************************************************************************************************************
Manifest Info Handler
***********************************************************************************************************************************/
#include "build.auto.h"

#include "common/debug.h"
#include "common/log.h"
#include "common/type/json.h"
#include "common/type/list.h"
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

STRING_STATIC(INFO_MANIFEST_SECTION_TARGET_PATH_STR,                "target:path");
STRING_STATIC(INFO_MANIFEST_SECTION_TARGET_PATH_DEFAULT_STR,        "target:path:default");

STRING_STATIC(INFO_MANIFEST_KEY_GROUP_STR,                          "group");
STRING_STATIC(INFO_MANIFEST_KEY_MODE_STR,                           "mode");
STRING_STATIC(INFO_MANIFEST_KEY_USER_STR,                           "user");

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
Create object from a file
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

        // Load the manifest
        Ini *iniLocal = NULL;
        this->info = infoNewLoad(storage, fileName, cipherType, cipherPass, &iniLocal);

        MEM_CONTEXT_TEMP_BEGIN()
        {
            // Load path defaults
            const String *userDefault = iniGetDefault(
                iniLocal, INFO_MANIFEST_SECTION_TARGET_PATH_DEFAULT_STR, INFO_MANIFEST_KEY_USER_STR, NULL);
            const String *groupDefault = iniGetDefault(
                iniLocal, INFO_MANIFEST_SECTION_TARGET_PATH_DEFAULT_STR, INFO_MANIFEST_KEY_GROUP_STR, NULL);
            const String *modeDefault = iniGet(iniLocal, INFO_MANIFEST_SECTION_TARGET_PATH_DEFAULT_STR, INFO_MANIFEST_KEY_MODE_STR);

            // Load path list
            StringList *pathKeyList = iniSectionKeyList(iniLocal, INFO_MANIFEST_SECTION_TARGET_PATH_STR);

            for (unsigned int pathKeyIdx = 0; pathKeyIdx < strLstSize(pathKeyList); pathKeyIdx++)
            {
                const String *path = strLstGet(pathKeyList, pathKeyIdx);
                KeyValue *pathData = varKv(jsonToVar(iniGet(iniLocal, INFO_MANIFEST_SECTION_TARGET_PATH_STR, path)));

                // const String *user = iniGetDefault(
                //     iniLocal, INFO_MANIFEST_SECTION_TARGET_PATH_DEFAULT_STR, INFO_MANIFEST_KEY_USER_STR, NULL);
            }
        }
        MEM_CONTEXT_TEMP_END();
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_LOG_RETURN(INFO_MANIFEST, this);
}
