/***********************************************************************************************************************************
InfoArchive Handler for archive information
***********************************************************************************************************************************/
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "common/memContext.h"
#include "common/ini.h"
#include "info/infoArchive.h"
#include "info/infoDb.h"
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
    InfoDb *infoDb;                                                 // Contents of the DB data
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

        this->infoDb = infoDbNew(fileName, loadFile, ignoreMissing, infoDbArchive);
    }
    MEM_CONTEXT_NEW_END();

    // Return buffer
    return this;
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
