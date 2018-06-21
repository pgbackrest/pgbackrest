/***********************************************************************************************************************************
Info Handler for pgbackrest information
***********************************************************************************************************************************/
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "common/debug.h"
#include "common/ini.h"
#include "common/log.h"
#include "common/memContext.h"
#include "crypto/hash.h"
#include "info/info.h"
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
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(INFO, this);
        FUNCTION_TEST_PARAM(BOOL, ignoreError);

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_TEST_END();

    bool result = true;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Make sure the ini is valid by testing the checksum
        String *infoChecksum = varStr(iniGet(this->ini, strNew(INI_SECTION_BACKREST), strNew(INI_KEY_CHECKSUM)));

        CryptoHash *hash = infoHash(this->ini);
printf("HASH: %s, INFOCHECKSUM: %s\n", strPtr(strQuoteZ(cryptoHashHex(hash), "\"")), strPtr(infoChecksum)); fflush(stdout);
        // ??? Temporary hack until get json parser: add quotes around hash before comparing
        if (infoChecksum == NULL || !strEq(infoChecksum, strQuoteZ(cryptoHashHex(hash), "\"")))
        {
            if (!ignoreError)
            {
                THROW_FMT(
                    ChecksumError, "invalid checksum in '%s', expected '%s' but found '%s'",
                    strPtr(this->fileName), strPtr(cryptoHashHex(hash)), infoChecksum == NULL ? "[undef]" : strPtr(infoChecksum));
            }
            else
                result = false;
        }

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
                result = false;
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_TEST_RESULT(BOOL, result);
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
printf("PARSING: %s\n", strPtr(fileName)); fflush(stdout);
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
                // ??? This is current perl behavior - it may be time to reconsider:
                // If only the info main file exists but is invalid, invalid errors will not be thrown. Instead a "missing" error
                // will be thrown, so if a missing error occurs, it can be that the files are both missing or the main file only
                // exists but is invalid.
                if (!ignoreMissing)
                {
                    THROW_FMT(
                        FileMissingError, "unable to open %s or %s",
                        strPtr(this->fileName), strPtr(strCat(strDup(this->fileName), INI_COPY_EXT)));
                }
            }
            else
                this->exists = true;
        }
        else
            this->exists = true;
    }
    MEM_CONTEXT_NEW_END();

    // Return buffer
    FUNCTION_DEBUG_RESULT(INFO, this);
}

/***********************************************************************************************************************************
Return a hash of the contents of the info file
***********************************************************************************************************************************/
CryptoHash *
infoHash(Ini *ini)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(INI, ini);

        FUNCTION_TEST_ASSERT(ini != NULL);
    FUNCTION_TEST_END();

    CryptoHash *result = cryptoHashNew(strNew(HASH_TYPE_SHA1));
CryptoHash *testHash =  cryptoHashNew(strNew(HASH_TYPE_SHA1));


    MEM_CONTEXT_TEMP_BEGIN()
    {
        StringList *sectionList = iniSectionList(ini);
String *test = strNew("\n");

        // Loop through sections and create hash for checking checksum
        for (unsigned int sectionIdx = 0; sectionIdx < strLstSize(sectionList); sectionIdx++)
        {
            String *section = strLstGet(sectionList, sectionIdx);
printf("SECTION: %s\n", strPtr(section)); fflush(stdout);
            // Add extra linefeed before additional sections
            if (sectionIdx != 0)
{
test=strCatFmt(test, "\n");
                cryptoHashProcessC(result, (const unsigned char *)"\n", 1);
}

            // Create the section header
            cryptoHashProcessC(result, (const unsigned char *)"[", 1);
            cryptoHashProcessStr(result, section);
            cryptoHashProcessC(result, (const unsigned char *)"]\n", 2);

test=strCatFmt(test, "[%s]\n", strPtr(section));

            StringList *keyList = iniSectionKeyList(ini, section);

            // Loop through values and build the section
            for (unsigned int keyIdx = 0; keyIdx < strLstSize(keyList); keyIdx++)
            {
                String *key = strLstGet(keyList, keyIdx);

                // Skip the backrest checksum in the file
                if ((strEq(section, strNew(INI_SECTION_BACKREST)) && !strEq(key, strNew(INI_KEY_CHECKSUM))) ||
                    !strEq(section, strNew(INI_SECTION_BACKREST)))
                {
printf("KEY=VALUE: %s=%s\n", strPtr(key), strPtr(varStr(iniGet(ini, section, strLstGet(keyList, keyIdx))))); fflush(stdout);
                    cryptoHashProcessStr(result, key);
                    cryptoHashProcessC(result, (const unsigned char *)"=", 1);
                    cryptoHashProcessStr(result, varStr(iniGet(ini, section, strLstGet(keyList, keyIdx))));
                    cryptoHashProcessC(result, (const unsigned char *)"\n", 1);
test=strCatFmt(test, "%s=%s\n", strPtr(key), strPtr(varStr(iniGet(ini, section, strLstGet(keyList, keyIdx)))));
                }
            }
        }

// printf("TEST: %s\n", strPtr(test)); fflush(stdout);
cryptoHashProcessStr(testHash, test);
printf("TEST: %s, HASH: %s\n", strPtr(test), strPtr(cryptoHashHex(testHash))); fflush(stdout);
    }
    MEM_CONTEXT_TEMP_END();


    FUNCTION_TEST_RESULT(CRYPTO_HASH, result);
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
Ini *
infoIni(const Info *this)
{
    FUNCTION_DEBUG_BEGIN(logLevelDebug);
        FUNCTION_DEBUG_PARAM(INFO, this);

        FUNCTION_DEBUG_ASSERT(this != NULL);
    FUNCTION_DEBUG_END();

    FUNCTION_DEBUG_RESULT(INI, this->ini);
}

String *
infoFileName(const Info *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(INFO, this);

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RESULT(STRING, this->fileName);
}

bool
infoExists(const Info *this)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(INFO, this);

        FUNCTION_DEBUG_ASSERT(this != NULL);
    FUNCTION_DEBUG_END();

    FUNCTION_DEBUG_RESULT(BOOL, this->exists);
}
