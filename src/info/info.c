/***********************************************************************************************************************************
Info Handler for pgbackrest information
***********************************************************************************************************************************/
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "common/debug.h"
#include "common/log.h"
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
    // FUNCTION_DEBUG_BEGIN(logLevelTrace);
    //     FUNCTION_DEBUG_PARAM(INFO, this);
    //     FUNCTION_DEBUG_PARAM(BOOL, ignoreError);
    //
    //     FUNCTION_DEBUG_ASSERT(this != NULL);
    // FUNCTION_DEBUG_END();
    // CSHANG Unable to have the Function Debug statements in this function:
    //     run 001 - info
    //         l0031 - expect FileMissingError: unable to open /home/vagrant/test/test-0/test.ini or /home/vagrant/test/test-0/test.ini.copy
    //         l0032 - new info - no files, ignore missing
    //         l0033 -     exists is false
    //         l0034 -     info filename is set
    //         l0039 - put info.copy to file
    //         l0041 - new info - load copy file
    //
    //     TEST FAILED WITH AssertError:
    //
    //     --------------------------------------------------------------------------------
    //     statement 'infoNew(fileName, false)' threw error AssertError, 'popping info/info.c:infoValidInternal but expected common/ini.c:iniGet' but result expected
    //     --------------------------------------------------------------------------------
    //
    //     THROWN AT:
    //     module/info/infoTest:testRun:41:(test build required for parameters)
    //         ... function(s) ommitted ...
    //     test:main:(argListSize: 1, argList: (char *[]))


    bool result = true;

    // ??? Need to add in checksum validation as first check
    // ?? Also, would it be better to pass in the filename? This and the old code will throw errors with the main file name even
    //   though erroring on the copy

    // Make sure that the format is current, otherwise error
    if (varIntForce(iniGet(this->ini, strNew(INI_SECTION_BACKREST), strNew(INI_KEY_FORMAT))) != PGBACKREST_FORMAT)
    {
        if (!ignoreError)
        {
            THROW_FMT(
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

    // FUNCTION_DEBUG_RESULT(BOOL, result);  // CSHANG
}

/***********************************************************************************************************************************
Internal function to load the copy and check validity
***********************************************************************************************************************************/
static bool
loadInternal(Info *this, const bool copyFile)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(INFO, this);
        FUNCTION_DEBUG_PARAM(BOOL, copyFile);

        FUNCTION_DEBUG_ASSERT(this != NULL);
    FUNCTION_DEBUG_END();

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

    FUNCTION_DEBUG_RESULT(BOOL, result);
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
    // CSHANG How do these actually get output? I ran the following but did not see infoNew or anything from this module:
    // /backrest/test/test.pl --dev --vm-out --vm=u18 --module=info --test=info --log-level=debug

    FUNCTION_DEBUG_BEGIN(logLevelDebug);
        FUNCTION_DEBUG_PARAM(STRING, fileName);
        FUNCTION_DEBUG_PARAM(BOOL, ignoreMissing);

        FUNCTION_DEBUG_ASSERT(fileName != NULL);
    FUNCTION_DEBUG_END();

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
                    THROW_FMT(
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

// CSHANG So will this just log the pointer when log level is debug?
    // Return buffer
    FUNCTION_DEBUG_RESULT(INFO, this);
}

Ini *
infoIni(const Info *this)
{
    FUNCTION_DEBUG_BEGIN(logLevelDebug);
        FUNCTION_DEBUG_PARAM(INFO, this);

        FUNCTION_DEBUG_ASSERT(this != NULL);
    FUNCTION_DEBUG_END();

    FUNCTION_DEBUG_RESULT(INI, this->ini);
}

/***********************************************************************************************************************************
Free the info
***********************************************************************************************************************************/
void
infoFree(Info *this)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(INFO, this);
    FUNCTION_DEBUG_END();

    if (this != NULL)
        memContextFree(this->memContext);

    FUNCTION_DEBUG_RESULT_VOID();
}


/***********************************************************************************************************************************
Accessor functions
***********************************************************************************************************************************/
String *
infoFileName(const Info *this)
{
    // CSHANG These only get output if an error occurs in our development environment, so maybe these should be FUNCTION_DEBUG
    // instead so we can see the filename in a production log (assuming is it displayed) and also be able to debug if a NULL pointer
    // was passed.
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(INFO, this);

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RESULT(STRING, this->fileName);
}

bool
infoExists(const Info *this)
{
    // CSHANG same: FUNCTION_DEBUG vs TEST
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(INFO, this);

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RESULT(BOOL, this->exists);
}
