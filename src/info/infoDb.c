/***********************************************************************************************************************************
Info Handler for pgbackrest information
***********************************************************************************************************************************/
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "common/memContext.h"
#include "common/ini.h"
#include "storage/helper.h"

/***********************************************************************************************************************************
Internal constants
***********************************************************************************************************************************/

/***********************************************************************************************************************************
Contains information about the info
***********************************************************************************************************************************/
struct InfoDb
{
    MemContext *memContext;                                         // Context that contains the infoDb
    Info *info;                                                     // Info contents
};

/***********************************************************************************************************************************
Create a new InfoDb object
***********************************************************************************************************************************/
InfoDb *
infoDbNew(String *fileName, const Storage *storage, const bool loadFile, const bool ignoreMissing)
{
    InfoDb *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("infoDb")
    {
        // Create object
        this = memNew(sizeof(InfoDb));
        this->memContext = MEM_CONTEXT_NEW();

        this->info = infoNew();

        Ini *infoIni = infoIni(this->info);

        if (!infoIni->exists)
        {

        }
    }
    MEM_CONTEXT_NEW_END();

    // Return buffer
    return this;
}

/***********************************************************************************************************************************
Free the info
***********************************************************************************************************************************/
void
infoDbFree(InfoDb *this)
{
    if (this != NULL)
        memContextFree(this->memContext);
}
