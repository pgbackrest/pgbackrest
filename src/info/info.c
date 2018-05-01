/***********************************************************************************************************************************
Info Handler for pgbackrest information
***********************************************************************************************************************************/
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include <stdio.h> // CSHANG - remove - needed for running info-pg test else implicit declaration of printf

#include "common/memContext.h"
#include "info/info.h"
#include "common/ini.h"
#include "storage/helper.h"
#include "version.h"

/***********************************************************************************************************************************
Internal constants
***********************************************************************************************************************************/
#define INI_COPY_EXT                                                ".copy"
#define INI_SECTION_BACKREST                                        "backrest"
#define INI_KEY_FORMAT                                              "backrest-format"
#define INI_KEY_VERSION                                             "backrest-version"
#define INI_KEY_CHECKSUM                                            "backrest-checksum"

/***********************************************************************************************************************************
Contains information about the info
***********************************************************************************************************************************/
struct Info
{
    MemContext *memContext;                                         // Context that contains the info
    String *backrestChecksum;                                       // pgBackRest checksum
    int backrestFormat;                                             // pgBackRest format number
    String *backrestVersion;                                        // pgBackRest Version
    // bool modified;   CSHANG may need later                       // Has the data been modified since last load/save?
    Ini *ini;                                                       // Parsed file contents
};


/***********************************************************************************************************************************
Internal function to check if the information is valid or not
***********************************************************************************************************************************/
bool
infoValidInternal(Ini *ini, const bool ignoreError)
{
    bool result = true;

    // CSHANG Need to add in checksum validation as first check

    // Make sure that the format is current, otherwise error
    if (varIntForce(iniGet(ini, strNew(INI_SECTION_BACKREST), strNew(INI_KEY_FORMAT))) != PGBACKREST_FORMAT)
    {
        if (!ignoreError)
        {
            THROW(
                FormatError, "invalid format in '%s', expected %d but found %d",
                strPtr(iniFileName(ini)), PGBACKREST_FORMAT, varIntForce(iniGet(ini, strNew(INI_SECTION_BACKREST),
                strNew(INI_KEY_FORMAT))));
        }
        else
        {
            result = false;
        }
    }

    // Check if the version has changed - update it if different so we know which version of backrest is saving the file
    if (!strEqZ(varStr(iniGet(ini, strNew(INI_SECTION_BACKREST), strNew(INI_KEY_VERSION))), PGBACKREST_VERSION))
    {
printf("CALLING INISET\n"); fflush(stdout); // CSHANG - remove
        iniSet(ini, strNew(INI_SECTION_BACKREST), strNew(INI_KEY_VERSION), varNewStrZ(PGBACKREST_VERSION));
printf("VERSION VARTYPE ADDED %d\n", varType(iniGet(ini, strNew(INI_SECTION_BACKREST), strNew(INI_KEY_VERSION))));fflush(stdout); // CSHANG - remove
    }

    return result;
}

/***********************************************************************************************************************************
Create a new Info object
CSHANG iniLoad extpects the file to exist since no ignoreMissing - need to change that or at least here since need to eventually
       not require it to exist. Maybe also then change parse.c
CSHANG Also need to be able to load a string - so optional strContent param needed?
CSHANG need to handle modified flag, encryption and checksum - but checksum can't be set until the file is saved
***********************************************************************************************************************************/
Info *
infoNew(String *fileName, const bool loadFile, const bool ignoreMissing)
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
            // CSHANG iniLoad should take an "ignore missing"  and should set the exists flag since if doesn't exist, then don't
            // call infoValidInternal
            iniLoad(this->ini, fileName);

            // If the main file does not exist and ignore missing is set, or if the main is in valid, try the copy file
            if ((!iniFileExists(this->ini) && ignoreMissing) || !infoValidInternal(this->ini, true))
            {
                iniLoad(this->ini, strCat(strDup(fileName), INI_COPY_EXT));
                if (!infoValidInternal(this->ini, false))
                    if (!ignoreMissing)
                        THROW(
                            FileMissingError, "unable to open %s or %s", strPtr(fileName),
                            strPtr(strCat(strDup(fileName), INI_COPY_EXT)));
            }

            // Set the data for external use (e.g. backup:current)
            // this->backrestVersion = varStr(iniGet(this->ini, strNew(INI_SECTION_BACKREST), strNew(INI_KEY_VERSION)));
printf("VERSION VARTYPE %d\n", varType(iniGet(this->ini, strNew(INI_SECTION_BACKREST), strNew(INI_KEY_VERSION))));fflush(stdout); // CSHANG - remove
// VARTYPE IS 6 == varTypeKeyValue so  error with 'variant type is not string' so iniSet calls kvAdd which is changing it from 5 to 6
printf("chkVARTYPE %d\n", varType(iniGet(this->ini, strNew(INI_SECTION_BACKREST), strNew(INI_KEY_CHECKSUM))));fflush(stdout); // CSHANG - remove
            this->backrestChecksum = varStr(iniGet(this->ini, strNew(INI_SECTION_BACKREST), strNew(INI_KEY_CHECKSUM)));
            this->backrestFormat = varIntForce(iniGet(this->ini, strNew(INI_SECTION_BACKREST), strNew(INI_KEY_FORMAT)));

            // CSHANG Do we need to check the two files against each other? We don't now - we just pick the first that appears valid
            // given the header and the checksum
        }

        // If there is no content to load then initialize the data
        if (!iniFileExists(this->ini))
        {
            // CSHANG How will we be storing this? Do we need to set both the ini AND the this->backrestVersion, etc?
            iniSet(this->ini, strNew(INI_SECTION_BACKREST), strNew(INI_KEY_VERSION), varNewStrZ(PGBACKREST_VERSION));
            iniSet(this->ini, strNew(INI_SECTION_BACKREST), strNew(INI_KEY_FORMAT), varNewInt(PGBACKREST_FORMAT));

            // Set the data for external use (e.g. backup:current)
            this->backrestVersion = varStr(iniGet(this->ini, strNew(INI_SECTION_BACKREST), strNew(INI_KEY_VERSION)));
            this->backrestFormat = varIntForce(iniGet(this->ini, strNew(INI_SECTION_BACKREST), strNew(INI_KEY_FORMAT)));
        }
    }
    MEM_CONTEXT_NEW_END();

    // Return buffer
    return this;
}

Ini *
infoIni(const Info *this)
{
    return this->ini;
}

/***********************************************************************************************************************************
Internal function to initialize the header information
***********************************************************************************************************************************/
// void
// infoInitInternal(Info *this)
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
// infoLoad(Ini *ini, String *fileName, const bool ignoreMissing)
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
// infoLoadFile(Info *this, String *fileName, const bool loadCopy, const bool ignoreMissing)
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
