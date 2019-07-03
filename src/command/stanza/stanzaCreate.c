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
#include "common/encode.h"
#include "common/encode/base64.h"
#include "common/io/handleWrite.h"
#include "common/log.h"
#include "common/memContext.h"
#include "config/config.h"
#include "info/info.h"
#include "info/infoArchive.h"
#include "info/infoBackup.h"
#include "info/infoPg.h"
#include "postgres/interface.h"
#include "postgres/version.h"
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
        // CSHANG Does this mean we can only run the c version of stanza-create where the db is local (i.e. repo and db on same machine)? But can do a "get" and then could read pgControl from buffer - WAIT that's exactly what pgControlFromFile does...
        PgControl pgControl = pgControlFromFile(cfgOptionStr(cfgOptPgPath));

        // If neither archive info nor backup info files exist then create the stanza
        if (!storageExistsNP(storageRepo(), STRDEF(STORAGE_REPO_ARCHIVE "/" INFO_ARCHIVE_FILE)) &&
            !storageExistsNP(storageRepo(), STRDEF(STORAGE_REPO_ARCHIVE "/" INFO_ARCHIVE_FILE INFO_COPY_EXT)) &&
            !storageExistsNP(storageRepo(), STRDEF(STORAGE_REPO_BACKUP "/" INFO_BACKUP_FILE)) &&
            !storageExistsNP(storageRepo(), STRDEF(STORAGE_REPO_BACKUP "/" INFO_BACKUP_FILE INFO_COPY_EXT)))
        {
            String *cipherPassSub = NULL;

            // If the repo is encrypted, generate a cipher passphrase for encrypting subsequent files
            if (cipherType(cfgOptionStr(cfgOptRepoCipherType)) != cipherTypeNone)
            {
                unsigned char buffer[48]; // 48 is the amount of entropy needed to get a 64 base key
                cryptoRandomBytes(buffer, sizeof(buffer));
                char cipherPassSubChar[64];
                encodeToStr(encodeBase64, buffer, sizeof(buffer), cipherPassSubChar);
                cipherPassSub = strNew(cipherPassSubChar);
            }

            // Create and save archive info
            InfoArchive *infoArchive = infoArchiveNew(
                pgControl.version, pgControl.systemId, cipherType(cfgOptionStr(cfgOptRepoCipherType)), cipherPassSub);
            infoArchiveSave(infoArchive, storageRepoWrite(), STRDEF(STORAGE_REPO_ARCHIVE "/" INFO_ARCHIVE_FILE),
                cipherType(cfgOptionStr(cfgOptRepoCipherType)), cfgOptionStr(cfgOptRepoCipherPass));

            // Create and save backup info
            InfoBackup *infoBackup = infoBackupNew(pgControl.version, pgControl.systemId, pgControl.controlVersion,
                pgControl.catalogVersion, cipherType(cfgOptionStr(cfgOptRepoCipherType)), cipherPassSub);
            infoBackupSave(infoBackup, storageRepoWrite(), STRDEF(STORAGE_REPO_BACKUP "/" INFO_BACKUP_FILE),
                cipherType(cfgOptionStr(cfgOptRepoCipherType)), cfgOptionStr(cfgOptRepoCipherPass));

        }
        // // Else if both info files exist (in some form), then if valid (check PG? checksum?), just return
        // else if ((storageExistsNP(storageRepo(), STRDEF(STORAGE_REPO_ARCHIVE "/" INFO_ARCHIVE_FILE)) ||
        //     storageExistsNP(storageRepo(), STRDEF(STORAGE_REPO_ARCHIVE "/" INFO_ARCHIVE_FILE INFO_COPY_EXT))) &&
        //     (storageExistsNP(storageRepo(), STRDEF(STORAGE_REPO_BACKUP "/" INFO_BACKUP_FILE)) ||
        //     storageExistsNP(storageRepo(), STRDEF(STORAGE_REPO_BACKUP "/" INFO_BACKUP_FILE INFO_COPY_EXT))))
        // {
        // }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}
