/***********************************************************************************************************************************
Stanza Create Command
***********************************************************************************************************************************/
#include "build.auto.h"

#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "command/stanza/stanzaCreate.h"
#include "common/debug.h"
#include "common/io/handleWrite.h"
#include "common/log.h"
#include "common/memContext.h"
#include "config/config.h"
#include "info/info.h"
#include "info/infoArchive.h"
#include "info/infoBackup.h"
#include "info/infoPg.h"
#include "postgres/interface.h"
#include "storage/helper.h"

/***********************************************************************************************************************************
Constants
***********************************************************************************************************************************/



/***********************************************************************************************************************************
Render the information for the stanza based on the command parameters.
***********************************************************************************************************************************/
static String *
infoRender(void)
{
    FUNCTION_LOG_VOID(logLevelDebug);

    String *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {


        memContextSwitch(MEM_CONTEXT_OLD());
        result = strDup(resultStr);
        memContextSwitch(MEM_CONTEXT_TEMP());
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(STRING, result);
}

/***********************************************************************************************************************************
Process stanza-create
***********************************************************************************************************************************/
void
cmdStanzaCreate(void)
{
    FUNCTION_LOG_VOID(logLevelDebug);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // ??? Temporary until can communicate with PG: Get control info from the pgControlFile
        PgControl pgControl = pgControlFromFile(cfgOptionStr(cfgOptPgPath));

        // CSHANG Do I need to initialize repo storage first?
        // If neither archive info or backup info files exist
        // if (!storageExistsNP(storageRepo(), STRDEF(STORAGE_REPO_ARCHIVE "/" INFO_ARCHIVE_FILE)) &&
        //     !storageExistsNP(storageRepo(), STRDEF(STORAGE_REPO_ARCHIVE "/" INFO_ARCHIVE_FILE INFO_COPY_EXT)))
        // {
        // CSHANG OR test existence by catching FileMissing error?
                // // Catch certain errors
                // TRY_BEGIN()
                // {
                // // Attempt to load the archive info file
                // InfoArchive *info = infoArchiveNew(
                //     storageRepo(), STRDEF(STORAGE_REPO_ARCHIVE "/" INFO_ARCHIVE_FILE), false, cipherType(cfgOptionStr(cfgOptRepoCipherType)), cfgOptionStr(cfgOptRepoCipherPass));
                // }
                // CATCH(FileMissingError)
                // {
                // }
                // TRY_END();

        // CSHANG Now we need to create the file - which we haven't done in c before. No other command should be creating the info files (expire updates backup.info)
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}
