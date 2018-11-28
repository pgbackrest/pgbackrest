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
        errorMsg = strNewFmt(
            "WAL segment version %s does not match archive version %s", strPtr(pgVersionToStr(pgVersion)),
            strPtr(pgVersionToStr(archivePg.version)));
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

/* CSHANG What we need here is an archiveId list - for the info command, if we stop at the newest then we aren't getting a complete picture. So if the archive.info and backup.info have:
archive.info
[db:history]
1={"db-id":6569239123849665679,"db-version":"9.4"}
2={"db-id":6569239123849665666,"db-version":"9.3"}
3={"db-id":6569239123849665679,"db-version":"9.4"}

backup.info
[backup:current]
20181116-154756F={"backrest-format":5,"backrest-version":"2.04","backup-archive-start":"000000010000000000000001","backup-archive-stop":"000000010000000000000002","backup-info-repo-size":2369190,"backup-info-repo-size-delta":2369190,"backup-info-size":20162900,"backup-info-size-delta":20162900,"backup-timestamp-start":1542383276,"backup-timestamp-stop":1542383289,"backup-type":"full","db-id":1,"option-archive-check":true,"option-archive-copy":false,"option-backup-standby":false,"option-checksum-page":true,"option-compress":true,"option-hardlink":false,"option-online":true}

[db:history]
1={"db-catalog-version":201409291,"db-control-version":942,"db-system-id":6569239123849665679,"db-version":"9.4"}
2={"db-catalog-version":201306121,"db-control-version":937,"db-system-id":6569239123849665666,"db-version":"9.3"}
3={"db-catalog-version":201409291,"db-control-version":942,"db-system-id":6569239123849665679,"db-version":"9.4"}
----
Then we will never get the info for db-id=1 and since there is backup data there, then we'll lose that info.
So maybe given the systemId and version, we also pass the history id. Or maybe pass an array of the history, system and version and if they do not match then go find the newest match? If then do match, then return the version-historyId (so if pass HID=1, SYS=XXXX, VER=9.4 from backup and that matches archive's HIS, SYS and VER then use it. If it doesn't then get the newest in the list.
*/
/***********************************************************************************************************************************
Given a backrest history id and PG systemId and version, return the archiveId of the best PG match.
***********************************************************************************************************************************/
const String *
infoArchiveIdHistoryMatch(const InfoArchive *this, const unsigned int historyId, const unsigned int pgVersion, const uint64_t pgSystemId)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(INFO_ARCHIVE, this);
        FUNCTION_DEBUG_PARAM(UINT, historyId);
        FUNCTION_DEBUG_PARAM(UINT, pgVersion);
        FUNCTION_DEBUG_PARAM(UINT64, pgSystemId);

        FUNCTION_DEBUG_ASSERT(this != NULL);
    FUNCTION_DEBUG_END();

    String *archiveId = NULL;
    InfoPg *infoPg = infoArchivePg(this);

    // Search the history list, from newest to oldest
    for (unsigned int pgIdx = 0; pgIdx < infoPgDataTotal(infoPg); pgIdx++)
    {
        InfoPgData pgDataArchive = infoPgData(infoPg, pgIdx);
        // If there is an exact match with the history, system and version then get the archiveId and stop
        if (historyId == pgDataArchive.id && pgSystemId == pgDataArchive.systemId && pgVersion == pgDataArchive.version)
        {
            archiveId = infoPgArchiveId(infoPg, pgIdx);
            break;
        }
    }

    // If there was not an exact match, then search for the first matching database system-id and version
    if (archiveId == NULL)
    {
        for (unsigned int pgIdx = 0; pgIdx < infoPgDataTotal(infoPg); pgIdx++)
        {
            InfoPgData pgDataArchive = infoPgData(infoPg, pgIdx);
            if (pgSystemId == pgDataArchive.systemId && pgVersion == pgDataArchive.version)
            {
                archiveId = infoPgArchiveId(infoPg, pgIdx);
                break;
            }
        }
    }

    // If the archive id has not been found, then error
    if (archiveId == NULL)
    {
        THROW_FMT(
            ArchiveMismatchError,
            "unable to retrieve the archive id for database version '%s' and system-id '%" PRIu64 "'",
            strPtr(pgVersionToStr(pgVersion)), pgSystemId);
    }

    FUNCTION_TEST_RESULT(STRING, archiveId);
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
