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
    String *archiveIdCurrent;                                       // Current archive id
};

/***********************************************************************************************************************************
Create a new InfoArchive object
***********************************************************************************************************************************/
InfoArchive *
infoArchiveNew(String *fileName, const bool loadFile, const bool ignoreMissing)
{
    InfoArchive *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("infoArchive")
    {
        // Create object
        this = memNew(sizeof(InfoArchive));
        this->memContext = MEM_CONTEXT_NEW();

        this->infoPg = infoPgNew(fileName, loadFile, ignoreMissing, infoPgArchive);
        // get the archive id from here     // convert the version from an int to a string
        // InfoPgData currentPg = infoPgDataCurrent(this->infoPg);
    }
    MEM_CONTEXT_NEW_END();

    // Return buffer
    return this;
}

/***********************************************************************************************************************************
Return the current archive id
***********************************************************************************************************************************/
String *
infoArchiveArchiveIdCurrent(const InfoArchive *this)
{
    return this->archiveIdCurrent;
}


/***********************************************************************************************************************************
Free the info
***********************************************************************************************************************************/
void
infoArchiveFree(InfoArchive *this)
{
    if (this != NULL)
        memContextFree(this->memContext);
}
