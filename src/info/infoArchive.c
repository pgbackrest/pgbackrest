/***********************************************************************************************************************************
InfoArchive Handler for archive information
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
#include "storage/helper.h"

/***********************************************************************************************************************************
Internal constants
***********************************************************************************************************************************/

/***********************************************************************************************************************************
Contains information about the archive info
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
infoArchiveNew(String *fileName, const bool ignoreMissing)
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

        this->infoPg = infoPgNew(fileName, ignoreMissing, infoPgArchive);

        // Store the archiveId for the current PG db-version.db-id
        InfoPgData currentPg = infoPgDataCurrent(this->infoPg);

        this->archiveId = strNew("");
        strCatFmt(this->archiveId, "%s-%u", strPtr(infoPgVersionToString(currentPg.version)), currentPg.id);
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
infoArchiveCheckPg(const InfoArchive *this, const unsigned int pgVersion, uint64_t pgSystemId)
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
        errorMsg = strNewFmt("WAL segment version %u does not match archive version %u", pgVersion, archivePg.version);

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

/***********************************************************************************************************************************
Accessor functions
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
