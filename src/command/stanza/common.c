/***********************************************************************************************************************************
Stanza Commands Handler
***********************************************************************************************************************************/
#include "build.auto.h"

#include "command/check/common.h"
#include "common/debug.h"
#include "common/log.h"
#include "config/config.h"
#include "db/helper.h"
#include "info/infoPg.h"
#include "postgres/interface.h"
#include "postgres/version.h"
#include "storage/helper.h"

/**********************************************************************************************************************************/
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

        result = strNewEncode(encodeBase64, BUF(buffer, sizeof(buffer)));
    }

    FUNCTION_TEST_RETURN(result);
}

/**********************************************************************************************************************************/
PgControl
pgValidate(void)
{
    FUNCTION_TEST_VOID();

    PgControl result = {0};

    MEM_CONTEXT_TEMP_BEGIN()
    {
        if (cfgOptionBool(cfgOptOnline))
        {
            // Check the primary connections (and standby, if any) and return the primary database object.
            DbGetResult dbObject = dbGet(false, true, false);

            // Get the pgControl information from the pg*-path deemed to be the primary
            result = pgControlFromFile(storagePgIdx(dbObject.primaryIdx));

            // Check the user configured path and version against the database
            checkDbConfig(result.version, dbObject.primaryIdx, dbObject.primary, false);
        }
        // If the database is not online, assume that pg1 is the primary
        else
            result = pgControlFromFile(storagePg());
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_TEST_RETURN(result);
}
