/***********************************************************************************************************************************
InfoArchive Handler for archive information
***********************************************************************************************************************************/
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

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
// CSHANG Need loadFile
***********************************************************************************************************************************/
InfoArchive *
infoArchiveNew(String *fileName, const bool ignoreMissing)
{
    InfoArchive *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("infoArchive")
    {
        // Create object
        this = memNew(sizeof(InfoArchive));
        this->memContext = MEM_CONTEXT_NEW();

        this->infoPg = infoPgNew(fileName, ignoreMissing, infoPgArchive);

        // Store the archiveId for the current PG db-version.db-id
        InfoPgData currentPg = infoPgDataCurrent(this->infoPg);
        strCatFmt(this->archiveId, "%s-%s", strPtr(infoPgVersionToString(currentPg.version)),
            strPtr(varStrForce(varNewUInt64(currentPg.systemId))));
    }
    MEM_CONTEXT_NEW_END();

    // Return buffer
    return this;
}

/***********************************************************************************************************************************
Return the current archive id
***********************************************************************************************************************************/
const String *
infoArchiveId(const InfoArchive *this)
{
    return this->archiveId;
}

// CSHANG Name of 'check' function is a bit misleading - maybe checkPg - but then that would be in infoPg and then the caller would
// just check the Db

/***********************************************************************************************************************************
Free the info
***********************************************************************************************************************************/
void
infoArchiveFree(InfoArchive *this)
{
    if (this != NULL)
        memContextFree(this->memContext);
}
