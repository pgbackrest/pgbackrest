/***********************************************************************************************************************************
Backup Info Handler
***********************************************************************************************************************************/
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "common/debug.h"
#include "common/log.h"
#include "common/memContext.h"
#include "common/ini.h"
#include "info/info.h"
#include "info/infoBackup.h"
#include "info/infoPg.h"
#include "storage/helper.h"

/***********************************************************************************************************************************
Internal constants
??? INFO_BACKUP_SECTION should be in a separate include since it will also be used when reading the manifest
***********************************************************************************************************************************/
#define INFO_BACKUP_SECTION                                         "backup"
#define INFO_BACKUP_SECTION_BACKUP_CURRENT                          INFO_BACKUP_SECTION ":current"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct InfoBackup
{
    MemContext *memContext;                                         // Context that contains the InfoBackup
    InfoPg *infoPg;                                                 // Contents of the DB data
    StringList *backupCurrentKey;                                   // List of backup labels in the backup:current section
};

/***********************************************************************************************************************************
Create a new InfoBackup object
// ??? Need loadFile parameter
// ??? Need to handle ignoreMissing = true
***********************************************************************************************************************************/
InfoBackup *
infoBackupNew(const Storage *storage, const String *fileName, bool ignoreMissing)
{
    FUNCTION_DEBUG_BEGIN(logLevelDebug);
        FUNCTION_DEBUG_PARAM(STRING, fileName);
        FUNCTION_DEBUG_PARAM(BOOL, ignoreMissing);

        FUNCTION_DEBUG_ASSERT(fileName != NULL);
    FUNCTION_DEBUG_END();

    InfoBackup *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("infoBackup")
    {
        // Create object
        this = memNew(sizeof(InfoBackup));
        this->memContext = MEM_CONTEXT_NEW();

        // Catch file missing error and add backup-specific hints before rethrowing
        TRY_BEGIN()
        {
            this->infoPg = infoPgNew(storage, fileName, infoPgBackup);
        }
        CATCH(FileMissingError)
        {
            THROW_FMT(
                FileMissingError,
                "%s\n"
                "HINT: backup.info does not exist and is required to perform a backup.\n"
                "HINT: has a stanza-create been performed?",
                errorMessage());
        }
        TRY_END();

        Ini *infoIni = infoPgIni(this->infoPg);
        if (strLstExists(iniSectionList(infoIni), strNew(INFO_BACKUP_SECTION_BACKUP_CURRENT)))
            this->backupCurrentKey = iniSectionKeyList(infoIni, strNew(INFO_BACKUP_SECTION_BACKUP_CURRENT));
    }
    MEM_CONTEXT_NEW_END();

    // Return buffer
    FUNCTION_DEBUG_RESULT(INFO_BACKUP, this);
}

/***********************************************************************************************************************************
Checks the backup info file's DB section against the PG version, system id, catolog and constrol version passed in and returns
the history id of the current PG database.
// ??? Should we still check that the file exists if it is required?
***********************************************************************************************************************************/
unsigned int
infoBackupCheckPg(
    const InfoBackup *this,
    unsigned int pgVersion,
    uint64_t pgSystemId,
    uint32_t pgCatalogVersion,
    uint32_t pgControlVersion)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(INFO_BACKUP, this);
        FUNCTION_DEBUG_PARAM(UINT, pgVersion);
        FUNCTION_DEBUG_PARAM(UINT64, pgSystemId);
        FUNCTION_DEBUG_PARAM(UINT32, pgCatalogVersion);
        FUNCTION_DEBUG_PARAM(UINT32, pgControlVersion);

        FUNCTION_DEBUG_ASSERT(this != NULL);
    FUNCTION_DEBUG_END();

    InfoPgData backupPg = infoPgDataCurrent(this->infoPg);

    if (backupPg.version != pgVersion || backupPg.systemId != pgSystemId)
        THROW(BackupMismatchError, strPtr(strNewFmt(
            "database version = %u, system-id %" PRIu64 " does not match backup version = %u, system-id = %" PRIu64 "\n"
            "HINT: is this the correct stanza?", pgVersion, pgSystemId, backupPg.version, backupPg.systemId)));

    if (backupPg.catalogVersion != pgCatalogVersion || backupPg.controlVersion != pgControlVersion)
    {
        THROW(BackupMismatchError, strPtr(strNewFmt(
            "database control-version = %" PRIu32 ", catalog-version %" PRIu32
            " does not match backup control-version = %" PRIu32 ", catalog-version = %" PRIu32 "\n"
            "HINT: this may be a symptom of database or repository corruption!",
            pgControlVersion, pgCatalogVersion, backupPg.controlVersion, backupPg.catalogVersion)));
    }

    FUNCTION_DEBUG_RESULT(UINT, backupPg.id);
}

/***********************************************************************************************************************************
Get PostgreSQL info
***********************************************************************************************************************************/
InfoPg *
infoBackupPg(const InfoBackup *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(INFO_BACKUP, this);

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RESULT(INFO_PG, this->infoPg);
}

/***********************************************************************************************************************************
Free the info
***********************************************************************************************************************************/
void
infoBackupFree(InfoBackup *this)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(INFO_BACKUP, this);
    FUNCTION_DEBUG_END();

    if (this != NULL)
        memContextFree(this->memContext);

    FUNCTION_DEBUG_RESULT_VOID();
}
