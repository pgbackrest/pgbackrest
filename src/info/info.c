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
#define INI_COPY_EXT                                       ".copy";

/***********************************************************************************************************************************
Contains information about the info
***********************************************************************************************************************************/
struct Info
{
    MemContext *memContext;                                         // Context that contains the info
    bool exists;                                                    // Does the file exist?
    bool modified;                                                  // Has the data been modified since last load/save?
    // String *backrestChecksum;                                       // pgBackRest checksum
    // unsigned int backrestFormat;                                    // pgBackRest format number
    // String *backrestVersion;                                        // pgBackRest Vresion
    Ini *ini;                                                       // Info file contents
};

/***********************************************************************************************************************************
Create a new Info object
***********************************************************************************************************************************/
Info *
infoNew(String *fileName, const Storage *storage, const bool loadFile, const bool ignoreMissing)
{
    Info *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("info")
    {
        // Create object
        this = memNew(sizeof(Info));
        this->memContext = MEM_CONTEXT_NEW();

        this->ini = iniNew();
        this->modified = false;

        if (loadFile)
        {
// CSHANG Can't use iniLoad because can't ignore missing so have to do this in 2 steps like had to do in parse.c
// BUT then how do we set the filename in the ini structure? It's only set in iniLoad - why?

// CSHANG I thought ignoreMissing was being changed to errorOnMissing


            // Load the info file if it was found
            if (buffer != NULL)
            {
                // Parse the ini file
                iniParse(this->ini, strNewBuf(buffer));

                this->exists = true;
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
Internal function to initialize the header information
***********************************************************************************************************************************/
void
infoInitInternal(Ini *this)
{
    //
}

/***********************************************************************************************************************************
Attemps to load at least one info file (main or .copy)
***********************************************************************************************************************************/
void
infoLoad(Ini *this, String *fileName, const bool ignoreMissing)
{
// CSHANG I feel like I should not have to pass filename - that it shouuld be set in the ini structure but there is is not set unless it is loaded from disk

    // If main was not loaded then try the copy
    if (infoLoadFile(false, true))
    {
        if (infoLoadFile(true, true))
        {
            return if ignoreMissing;

            THROW(
                FileMissingError, "unable to open %s or %s",
                strPtr(fileName), strPtr(fileName) INI_COPY_EXT);
        }
    }

    this->exists = true;
}

bool
infoLoadFile(Ini *this, String *fileName, const bool loadCopy, const bool ignoreMissing)
{
    // Load the ini file
    Buffer *buffer = storageGetNP(
        storageOpenReadP(storageLocal(), fileName, .ignoreMissing = ignoreMissing));
/////// CSHANG more to do and maybe this isn't the best way
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
