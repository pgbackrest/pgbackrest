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
    String *fileName;                                               // Full path name of the file
    bool exists;                                                    // Does the file exist?
    Ini *ini;                                                       // Parsed file contents
};


/***********************************************************************************************************************************
Internal function to check if the information is valid or not
***********************************************************************************************************************************/
static bool
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

    return result;
}

/***********************************************************************************************************************************
Internal function to load the copy and check validity
***********************************************************************************************************************************/
static void
loadCopyInternal(Info *this, Buffer *buffer, const String *fileName, const bool fileRequired, const bool ignoreMissing)
{
    buffer = storageGetNP(storageNewReadP(storageLocal(), fileName, .ignoreMissing = ignoreMissing));
    if (buffer == NULL && fileRequired)
        THROW(
            FileMissingError, "unable to open %s or %s", strPtr(fileName), strPtr(fileName));
    iniParse(this->ini, strNewBuf(buffer));

    // When validating the copy, do not ignore errors
    infoValidInternal(this->ini, false);
}

/***********************************************************************************************************************************
Create a new Info object
CSHANG Need loadFile parameter to be able to load from a string, and to handle modified flag, encryption and checksum
***********************************************************************************************************************************/
Info *
infoNew(const String *fileName, const bool fileRequired, const bool ignoreMissing)
{
    Info *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("info")
    {
        // Create object
        this = memNew(sizeof(Info));
        this->memContext = MEM_CONTEXT_NEW();

        this->ini = iniNew();

        // Load the file. Error if file is required to exist.
        Buffer *buffer = storageGetNP(storageNewReadP(storageLocal(), fileName, .ignoreMissing = ignoreMissing));

        if (buffer != NULL)
        {
            iniParse(this->ini, strNewBuf(buffer));

            // If the main is invalid, try the copy file
            if (!infoValidInternal(this->ini, true))
                loadCopyInternal(this, buffer, strCat(strDup(fileName), INI_COPY_EXT), fileRequired, ignoreMissing);
        }
        // If the main file does not exist and ignore missing is true (would have errorred before this if ignoreMissing was false)
        else
            loadCopyInternal(this, buffer, strCat(strDup(fileName), INI_COPY_EXT), fileRequired, ignoreMissing);

        this->fileName = strDup(fileName);
        this->exists = true;

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
Free the info
***********************************************************************************************************************************/
void
infoFree(Info *this)
{
    if (this != NULL)
        memContextFree(this->memContext);
}
