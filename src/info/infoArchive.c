/***********************************************************************************************************************************
Archive Info Handler
***********************************************************************************************************************************/
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "common/debug.h"
#include "common/log.h"
#include "common/memContext.h"
#include "common/ini.h"
#include "info/infoArchive.h"
#include "info/infoPg.h"
#include "postgres/interface.h"
#include "storage/helper.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct InfoArchive
{
    MemContext *memContext;                                         // Context that contains the InfoArchive
    InfoPg *infoPg;                                                 // Contents of the DB data
    String *archiveId;                                              // Archive id for the current PG version
};

/***********************************************************************************************************************************
Create a new InfoArchive object
// ??? Need loadFile parameter
***********************************************************************************************************************************/
InfoArchive *
infoArchiveNew(const Storage *storage, const String *fileName, bool ignoreMissing)
{
    FUNCTION_DEBUG_BEGIN(logLevelDebug);
        FUNCTION_DEBUG_PARAM(STRING, fileName);
        FUNCTION_DEBUG_PARAM(BOOL, ignoreMissing);

        FUNCTION_DEBUG_ASSERT(fileName != NULL);
    FUNCTION_DEBUG_END();

    InfoArchive *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("infoArchive")
    {
        // Create object
        this = memNew(sizeof(InfoArchive));
        this->memContext = MEM_CONTEXT_NEW();

        // Catch file missing error and add archive-specific hints before rethrowing
        TRY_BEGIN()
        {
            this->infoPg = infoPgNew(storage, fileName, infoPgArchive);
        }
        CATCH(FileMissingError)
        {
            THROW_FMT(
                FileMissingError,
                "%s\n"
                "HINT: archive.info does not exist but is required to push/get WAL segments.\n"
                "HINT: is archive_command configured in postgresql.conf?\n"
                "HINT: has a stanza-create been performed?\n"
                "HINT: use --no-archive-check to disable archive checks during backup if you have an alternate archiving scheme.",
                errorMessage());
        }
        TRY_END();

        // Store the archiveId for the current PG db-version db-id
        this->archiveId = infoPgArchiveId(this->infoPg, 0);
    }
    MEM_CONTEXT_NEW_END();

    // Return buffer
    FUNCTION_DEBUG_RESULT(INFO_ARCHIVE, this);
}

/***********************************************************************************************************************************
Checks the archive info file's DB section against the PG version and system id passed in
// ??? Should we still check that the file exists if it is required?
***********************************************************************************************************************************/
void
infoArchiveCheckPg(const InfoArchive *this, unsigned int pgVersion, uint64_t pgSystemId)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(INFO_ARCHIVE, this);
        FUNCTION_DEBUG_PARAM(UINT, pgVersion);
        FUNCTION_DEBUG_PARAM(UINT64, pgSystemId);

        FUNCTION_DEBUG_ASSERT(this != NULL);
    FUNCTION_DEBUG_END();

    String *errorMsg = NULL;

    InfoPgData archivePg = infoPgDataCurrent(this->infoPg);

    if (archivePg.version != pgVersion)
    {
        errorMsg = strNewFmt("WAL segment version %s does not match archive version %s",
            strPtr(pgVersionToStr(pgVersion)), strPtr(pgVersionToStr(archivePg.version)));
    }

    if (archivePg.systemId != pgSystemId)
    {
        errorMsg = errorMsg != NULL ? strCat(errorMsg, "\n") : strNew("");
        strCatFmt(errorMsg,
                "WAL segment system-id %" PRIu64 " does not match archive system-id %" PRIu64,
                pgSystemId, archivePg.systemId);
    }

    if (errorMsg != NULL)
    {
        errorMsg = strCatFmt(errorMsg, "\nHINT: are you archiving to the correct stanza?");
        THROW(ArchiveMismatchError, strPtr(errorMsg));
    }

    FUNCTION_DEBUG_RESULT_VOID();
}

/***********************************************************************************************************************************
Get the current archive id
***********************************************************************************************************************************/
const String *
infoArchiveId(const InfoArchive *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(INFO_ARCHIVE, this);

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RESULT(STRING, this->archiveId);
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
