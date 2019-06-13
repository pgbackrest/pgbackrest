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
// static String *
// infoRender(void)
// {
//     FUNCTION_LOG_VOID(logLevelDebug);
//
//     String *result = NULL;
//
//     MEM_CONTEXT_TEMP_BEGIN()
//     {
//
//
//         memContextSwitch(MEM_CONTEXT_OLD());
//         result = strDup(resultStr);
//         memContextSwitch(MEM_CONTEXT_TEMP());
//     }
//     MEM_CONTEXT_TEMP_END();
//
//     FUNCTION_LOG_RETURN(STRING, result);
// }

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
        // CSHANG Does this mean we can only run the c version of stanza-create where the db is local (i.e. repo and db on same machine)? But can do a get and the could read pgControl from buffer
        PgControl pgControl = pgControlFromFile(cfgOptionStr(cfgOptPgPath));

        // If neither archive info or backup info files exist
        if (!storageExistsNP(storageRepo(), STRDEF(STORAGE_REPO_ARCHIVE "/" INFO_ARCHIVE_FILE)) &&
            !storageExistsNP(storageRepo(), STRDEF(STORAGE_REPO_ARCHIVE "/" INFO_ARCHIVE_FILE INFO_COPY_EXT)) &&
            !storageExistsNP(storageRepo(), STRDEF(STORAGE_REPO_BACKUP "/" INFO_BACKUP_FILE)) &&
            !storageExistsNP(storageRepo(), STRDEF(STORAGE_REPO_BACKUP "/" INFO_BACKUP_FILE INFO_COPY_EXT)))
        {

        // CSHANG Now we need to create the file - which we haven't done in c before. No other command should be creating the info files (expire updates backup.info). For infoArchiveSave - but that then needs to change to not store the catalog/control and replace db-system-id with db-id? - OR are we somehow going to rewrite the file on an upgrade to now have the same format? And what about the backup:current section db-id? I think we should just use db-id since it's used more than db-system-id
        }
        // If both exist, check if valid
        else if ((storageExistsNP(storageRepo(), STRDEF(STORAGE_REPO_ARCHIVE "/" INFO_ARCHIVE_FILE)) ||
            storageExistsNP(storageRepo(), STRDEF(STORAGE_REPO_ARCHIVE "/" INFO_ARCHIVE_FILE INFO_COPY_EXT))) &&
            (storageExistsNP(storageRepo(), STRDEF(STORAGE_REPO_BACKUP "/" INFO_BACKUP_FILE)) ||
            storageExistsNP(storageRepo(), STRDEF(STORAGE_REPO_BACKUP "/" INFO_BACKUP_FILE INFO_COPY_EXT))))
        {
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}
