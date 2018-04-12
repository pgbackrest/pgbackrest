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
#define INI_SECTION_BACKREST                               "backrest";
#define INI_KEY_FORMAT                                     "backrest-format";
#define INI_KEY_VERSION                                    "backrest-version";

/***********************************************************************************************************************************
Contains information about the info
***********************************************************************************************************************************/
struct Info
{
    MemContext *memContext;                                         // Context that contains the info
    bool exists;                                                    // Does the file exist?
    bool modified;                                                  // Has the data been modified since last load/save?
    Ini *ini;                                                       // Parsed file contents
};

/***********************************************************************************************************************************
Create a new Info object
CSHANG iniLoad extpects the file to exist since no ignoreMissing - need to change that or at least here since need to eventually
       not require it to exist. Maybe also then change parse.c
CSHANG Also need to be able to load a string - so optional strContent param needed?
CSHANG need to handle modified flag, encryption and checksum - but checksum can't be set until the file is saved
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

        if (loadFile)
        {
            // CSHANG this really should be calling an infoLoad/fileLoad function but right now assuming main file always exists
            iniLoad(this->ini, fileName);

            // CSHANG Maybe exists should be set in iniLoad or infoLoad?
            this->exists = true;

            // CSHANG When checking for the info and the info.copy then the first call ignoreError would be true & second call false
            // CSHANG result is just a temporary variable
            const bool result = infoValidInternal(this->ini, false);
            //if (!infoValidInternal(this->ini, true))
            // CSHANG Need to check the two files against each other? Where does that responsibility lie? Will all files that have
            // an info section have a .copy file? Maybe have a flag?
        }

        // If there is no content to load then initialize the data
        if (!this->exists)
        {
            iniSet(ini, strNew(INI_SECTION_BACKREST), strNew(INI_KEY_VERSION), PGBACKREST_VERSION);
            iniSet(ini, strNew(INI_SECTION_BACKREST), strNew(INI_KEY_FORMAT), varNewInt(PGBACKREST_FORMAT));
        }
    }
    MEM_CONTEXT_NEW_END();

    // Return buffer
    return this;
}

Ini *
infoIni(const info *this)
{
    return this->ini;
}

/***********************************************************************************************************************************
Internal function to check if the information is valid or not
***********************************************************************************************************************************/
bool
infoValidInternal(const Ini *this, const bool ignoreError)
{
    // CSHANG Need to add in checksum validation as first check
    // CSHANG Need to add in whether invalid should be ignored - since we have 2 files, if one is invalid, then it can be ignored
    bool result = true;

    // Make sure that the format is current, otherwise error
    if (varInt(iniGet(ini, strNew(INI_SECTION_BACKREST), strNew(INI_KEY_FORMAT))) != PGBACKREST_FORMAT)
    {
        if (!ignoreError)
        {
            THROW(
                FormatError, "invalid format in '%s', expected %d but found %d",
                strPtr(this->fileName), PGBACKREST_FORMAT, varInt(iniGet(ini, strNew(INI_SECTION_BACKREST), strNew(INI_KEY_FORMAT))));
        }
        else
        {
            result = false;
        }
    }

    // Check if the version has changed - update it if different so we know which version of backrest is saving the file
    if (strPtr(varStr(iniGet(ini, strNew(INI_SECTION_BACKREST), strNew(INI_KEY_VERSION))))) != PGBACKREST_VERSION)
    {
        iniSet(ini, strNew(INI_SECTION_BACKREST), strNew(INI_KEY_VERSION), PGBACKREST_VERSION);
    }

    return result;
}

/***********************************************************************************************************************************
Internal function to initialize the header information
***********************************************************************************************************************************/
// void
// infoInitInternal(Ini *this)
// {
//     //
// }

/***********************************************************************************************************************************
Attemps to load at least one info file (main or .copy)
CSHANG Maybe setting the filename in the iniLoad is not the right place? Maybe iniLoad and save should take an StorageFile object
    b/c open/read/write return it. Get filename from storage object. Or maybe the iniLoad moves up into this info object.
CSHANG Maybe use the storageFile object here such that we do:
    StorageFile storageFileIni = storageOpenReadP(storageLocal(), fileName, .ignoreMissing = ignoreMissing);
    if (storageFileIni != NULL) then the fileName is in the storage object so
***********************************************************************************************************************************/
// void
// infoLoad(Ini *this, String *fileName, const bool ignoreMissing)
// {
//     // If main was not loaded then try the copy
//     if (infoLoadFile(false, true))
//     {
//         if (infoLoadFile(true, true))
//         {
//             if (!ignoreMissing)
//                 THROW(
//                     FileMissingError, "unable to open %s or %s",
//                     strPtr(fileName), strPtr(fileName) INI_COPY_EXT);
//         }
//     }
//
//     this->exists = true;
// }
//
// bool
// infoLoadFile(Ini *this, String *fileName, const bool loadCopy, const bool ignoreMissing)
// {
//     // CSHANG Need to add ignore missing
//     iniLoad(this->ini, fileName);
//
//     // Check the header but
//     if (!infoValidInternal(this->ini, ))
// }

/***********************************************************************************************************************************
Free the info
***********************************************************************************************************************************/
void
infoFree(Info *this)
{
    if (this != NULL)
        memContextFree(this->memContext);
}
