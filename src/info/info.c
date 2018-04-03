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
Contains information about the info
***********************************************************************************************************************************/
struct Info
{
    MemContext *memContext;                                         // Context that contains the info
    bool exists;                                                    // Does the file exist?
    // String *backrestChecksum;                                       // pgBackRest checksum
    // unsigned int backrestFormat;                                    // pgBackRest format number
    // String *backrestVersion;                                        // pgBackRest Vresion
    Ini *ini;                                                       // Info file contents
};

/***********************************************************************************************************************************
Create a new Info object
***********************************************************************************************************************************/
Info *
infoNew(String *fileName, const Storage *storage, const bool loadFile, const bool errorOnMissing)
{
    Info *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("info")
    {
        // Create object
        this = memNew(sizeof(Info));
        this->memContext = MEM_CONTEXT_NEW();

        this->ini = iniNew();

        if (loadFile)
        {
            iniLoad(this->ini, fileName);
        then iniGet the header info and check fmt num & chksum  (But keep checksum commented out for now)
    }
    MEM_CONTEXT_NEW_END();

    // Return buffer
    return this;
}

/***********************************************************************************************************************************
Internal function to check if the header information is valid or not
***********************************************************************************************************************************/
bool
infoHeaderCheckInternal(const Ini *this)
{
    //
}

/***********************************************************************************************************************************
Internal function to check if the header information is valid or not
***********************************************************************************************************************************/
void
infoInitInternal(const Ini *this)
{
    //
}


/***********************************************************************************************************************************
Free the info
***********************************************************************************************************************************/
void
infoFree(Info *this)
{
    if (this != NULL)
        memContextFree(this->memContext);
}
