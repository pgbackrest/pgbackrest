/***********************************************************************************************************************************
Info Handler for pgbackrest information
***********************************************************************************************************************************/
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

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
infoValidInternal(const Info *this, const bool ignoreError)
{
    bool result = true;

    // ??? Need to add in checksum validation as first check
    // ?? Also, would it be better to pass in the filename? This and the old code will throw errors with the main file name even
    //   though erroring on the copy

    // Make sure that the format is current, otherwise error
    if (varIntForce(iniGet(this->ini, strNew(INI_SECTION_BACKREST), strNew(INI_KEY_FORMAT))) != PGBACKREST_FORMAT)
    {
        if (!ignoreError)
        {
            THROW(
                FormatError, "invalid format in '%s', expected %d but found %d",
                strPtr(this->fileName), PGBACKREST_FORMAT, varIntForce(iniGet(this->ini, strNew(INI_SECTION_BACKREST),
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
static bool
loadInternal(Info *this, const bool copyFile)
{
    bool result = false;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        String *fileName = copyFile ? strCat(strDup(this->fileName), INI_COPY_EXT) : this->fileName;

        Buffer *buffer = storageGetNP(storageNewReadP(storageLocal(), fileName, .ignoreMissing = true));

        // If the file exists, parse and validate it
        if (buffer != NULL)
        {
            iniParse(this->ini, strNewBuf(buffer));

            // Do not ignore errors if the copy file is invalid
            if (infoValidInternal(this, (copyFile ? false : true)))
                result = true;
        }
    }
    MEM_CONTEXT_TEMP_END();

    return result;
}

/***********************************************************************************************************************************
Create a new Info object
// ??? Need loadFile parameter to be able to load from a string.
// ??? Need to handle modified flag, encryption and checksum, initialization, etc.
// ??? The current Perl code does not handle this, but should we consider if the main file is the only one that exists & is invalid
//     an "invalid format" should be returned vs "unable to open x or x.copy"?
***********************************************************************************************************************************/
Info *
infoNew(
    const String *fileName,                                         // Full path/filename to load
    const bool ignoreMissing)                                       // Ignore if the file is missing, else required
{
    Info *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("info")
    {
        // Create object
        this = memNew(sizeof(Info));
        this->memContext = MEM_CONTEXT_NEW();

        this->ini = iniNew();
        this->fileName = strDup(fileName);

        // Attempt to load the main file. If it does not exist or is invalid, try to load the copy.
        if (!loadInternal(this, false))
        {
            if (!loadInternal(this, true))
            {
                if (!ignoreMissing)
                    THROW(
                        FileMissingError, "unable to open %s or %s",
                        strPtr(this->fileName), strPtr(strCat(strDup(this->fileName), INI_COPY_EXT)));
            }
            else
                this->exists = true;
        }
        else
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


/***********************************************************************************************************************************
Accessor functions
***********************************************************************************************************************************/
String *
infoFileName(const Info *this)
{
    return this->fileName;
}

bool
infoExists(const Info *this)
{
    return this->exists;
}
