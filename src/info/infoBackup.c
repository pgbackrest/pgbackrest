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
#include "info/infoBackup.h"
#include "info/infoPg.h"
#include "storage/helper.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct InfoBackup
{
    MemContext *memContext;                                         // Context that contains the InfoArchive
    InfoPg *infoPg;                                                 // Contents of the DB data
    unsigned int pgId;                                              // The history id associated with the current PG
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

    InfoArchive *this = NULL;

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
    }
    MEM_CONTEXT_NEW_END();

    // Return buffer
    FUNCTION_DEBUG_RESULT(INFO_BACKUP, this);
}

/***********************************************************************************************************************************
Checks the backup info file's DB section against the PG version, system id, catolog and constrol version passed in
// ??? Should we still check that the file exists if it is required?
***********************************************************************************************************************************/
unsigned int
infoBackupCheckPg(
    const InfoBackup *this,
    unsigned int pgVersion,
    uint64_t pgSystemId,
    uint32_t catalogVersion,
    uint32_t controlVersion)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(INFO_ARCHIVE, this);
        FUNCTION_DEBUG_PARAM(UINT, pgVersion);
        FUNCTION_DEBUG_PARAM(UINT64, pgSystemId);
        FUNCTION_DEBUG_PARAM(UINT32, pgCatalogVersion);
        FUNCTION_DEBUG_PARAM(UINT32, pgControlVersion);

        FUNCTION_DEBUG_ASSERT(this != NULL);
    FUNCTION_DEBUG_END();

    String *errorMsg = NULL;

    InfoPgData backupPg = infoPgDataCurrent(this->infoPg);

    if (backupPg.version != pgVersion || backupPg.systemId != pgSystemId)
        THROW(BackupMismatchError, strPtr(strNewFmt(
            "database version = %u, system-id %" PRIu64 " does not match backup version = %u, system-id = %" PRIu64 "\n"
            "HINT: is this the correct stanza?", pgVersion, pgSystemId, backupPg.version, backupPg.systemId)));

    if (backupPg.catalogVersion != catalogVersion || backupPg.controlVersion != controlVersion)
    {
        THROW(BackupMismatchError, strPtr(strNewFmt(
            "database control-version = %" PRIu32 ", catalog-version %" PRIu32
            " does not match backup control-version = %" PRIu32 ", catalog-version = %" PRIu32 "\n"
            "HINT: this may be a symptom of database or repository corruption!",
            pgControlVersion, pgCatalogVersion, backupPg.controlVersion, backupPg.catalogVersion)));
    }
// CSHANG Hmmm not sure I like this since it should be set in NEW but then we'd have to call check from NEW - OR we set it in NEW but then check it here if it is different?
    this->pgId = backupPg.id;

    FUNCTION_DEBUG_RESULT(UINT, this->pgId);
}

/***********************************************************************************************************************************
Get the current pg id
***********************************************************************************************************************************/
const unsigned int
infoArchiveId(const InfoBackup *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(INFO_BACKUP, this);

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RESULT(UINT, this->pgId);
}

/***********************************************************************************************************************************
Get PostgreSQL info
***********************************************************************************************************************************/
InfoPg *
infoArchivePg(const InfoArchive *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(INFO_ARCHIVE, this);

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RESULT(INFO_PG, this->infoPg);
}

/***********************************************************************************************************************************
Free the info
***********************************************************************************************************************************/
void
infoArchiveFree(InfoArchive *this)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(INFO_ARCHIVE, this);
    FUNCTION_DEBUG_END();

    if (this != NULL)
        memContextFree(this->memContext);

    FUNCTION_DEBUG_RESULT_VOID();
}
