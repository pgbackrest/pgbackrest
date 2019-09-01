/***********************************************************************************************************************************
Stanza Commands Handler
***********************************************************************************************************************************/
#include "build.auto.h"

#include "command/check/common.h"
#include "common/debug.h"
#include "common/encode.h"
#include "common/log.h"
#include "config/config.h"
#include "db/helper.h"
#include "info/infoPg.h"
#include "postgres/interface.h"
#include "postgres/version.h"
#include "storage/helper.h"

/***********************************************************************************************************************************
Generate a cipher
***********************************************************************************************************************************/
String *
cipherPassGen(CipherType cipherType)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, cipherType);
    FUNCTION_TEST_END();

    String *result = NULL;

    if (cipherType != cipherTypeNone)
    {
        unsigned char buffer[48]; // 48 is the amount of entropy needed to get a 64 base key
        cryptoRandomBytes(buffer, sizeof(buffer));
        char cipherPassSubChar[64];
        encodeToStr(encodeBase64, buffer, sizeof(buffer), cipherPassSubChar);
        result = strNew(cipherPassSubChar);
    }

    FUNCTION_TEST_RETURN(result);
}

/***********************************************************************************************************************************
Validate the archive and backup info files
***********************************************************************************************************************************/
void
infoValidate(const InfoPgData *archiveInfo, const InfoPgData *backupInfo)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM_P(INFO_PG_DATA, archiveInfo);
        FUNCTION_TEST_PARAM_P(INFO_PG_DATA, backupInfo);
    FUNCTION_TEST_END();

    ASSERT(archiveInfo != NULL);
    ASSERT(backupInfo != NULL);

    // Error if there is a mismatch between the archive and backup info files
    if (archiveInfo->id != backupInfo->id || archiveInfo->systemId != backupInfo->systemId ||
        archiveInfo->version != backupInfo->version)
    {
        THROW_FMT(
            FileInvalidError, "backup info file and archive info file do not match\n"
            "archive: id = %u, version = %s, system-id = %" PRIu64 "\n"
            "backup : id = %u, version = %s, system-id = %" PRIu64 "\n"
            "HINT: this may be a symptom of repository corruption!",
            archiveInfo->id, strPtr(pgVersionToStr(archiveInfo->version)), archiveInfo->systemId, backupInfo->id,
            strPtr(pgVersionToStr(backupInfo->version)), backupInfo->systemId);
    }

    FUNCTION_TEST_RETURN_VOID();
}

/***********************************************************************************************************************************
Validate and return database information
***********************************************************************************************************************************/
PgControl
pgValidate(void)
{
    FUNCTION_TEST_VOID();

    PgControl result = {0};

    MEM_CONTEXT_TEMP_BEGIN()
    {
        if (cfgOptionBool(cfgOptOnline))
        {
            // Check the connections of the master (and standby, if any) and return the master database object.
            DbGetResult dbObject = dbGet(false, true);

            // Get the pgControl information from the pg*-path deemed to be the master
            result = pgControlFromFile(storagePgId(dbObject.primaryId), cfgOptionStr(cfgOptPgPath + dbObject.primaryId - 1));

            // Check the user configured path and version against the database
            checkDbConfig(result.version, dbObject.primaryId, dbPgVersion(dbObject.primary), dbPgDataPath(dbObject.primary));
        }
        // If the database is not online, assume that pg1 is the master
        else
            result = pgControlFromFile(storagePg(), cfgOptionStr(cfgOptPgPath));
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_TEST_RETURN(result);
}
