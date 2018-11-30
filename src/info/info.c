/***********************************************************************************************************************************
Info Handler
***********************************************************************************************************************************/
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "common/debug.h"
#include "common/ini.h"
#include "common/log.h"
#include "common/memContext.h"
#include "crypto/cipherBlock.h"
#include "crypto/hash.h"
#include "info/info.h"
#include "storage/helper.h"
#include "version.h"

/***********************************************************************************************************************************
Internal constants
***********************************************************************************************************************************/
#define INI_COPY_EXT                                                ".copy"

// CSHANG I think all the #defines below should be removed - there is no need for them. More often than not we will not need the #define - it is more the exception than the rule. Also I think we should change the name to be INFO_ vs INI_.
#define INI_SECTION_BACKREST                                        "backrest"
    STRING_STATIC(INI_SECTION_BACKREST_STR,                         INI_SECTION_BACKREST);
#define INI_SECTION_CIPHER                                          "cipher"
    STRING_STATIC(INI_SECTION_CIPHER_STR,                           INI_SECTION_CIPHER);

#define INI_KEY_CIPHER_PASS                                         "cipher-pass"
    STRING_STATIC(INI_KEY_CIPHER_PASS_STR,                          INI_KEY_CIPHER_PASS);
#define INI_KEY_CHECKSUM                                            "backrest-checksum"
    STRING_STATIC(INI_KEY_CHECKSUM_STR,                             INI_KEY_CHECKSUM);
STRING_EXTERN(INI_KEY_FORMAT_STR,                                   "backrest-format");
STRING_EXTERN(INI_KEY_VERSION_STR,                                  "backrest-version");

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct Info
{
    MemContext *memContext;                                         // Context that contains the info
    String *fileName;                                               // Full path name of the file
    Ini *ini;                                                       // Parsed file contents
    const String *cipherPass;                                       // Cipher passphrase if set
};

/***********************************************************************************************************************************
Return a hash of the contents of the info file
***********************************************************************************************************************************/
static CryptoHash *
infoHash(const Ini *ini)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(INI, ini);

        FUNCTION_TEST_ASSERT(ini != NULL);
    FUNCTION_TEST_END();

    CryptoHash *result = cryptoHashNew(HASH_TYPE_SHA1_STR);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        StringList *sectionList = iniSectionList(ini);

        // Initial JSON opening bracket
        cryptoHashProcessC(result, (const unsigned char *)"{", 1);

        // Loop through sections and create hash for checking checksum
        for (unsigned int sectionIdx = 0; sectionIdx < strLstSize(sectionList); sectionIdx++)
        {
            String *section = strLstGet(sectionList, sectionIdx);

            // Add a comma before additional sections
            if (sectionIdx != 0)
                cryptoHashProcessC(result, (const unsigned char *)",", 1);

            // Create the section header
            cryptoHashProcessC(result, (const unsigned char *)"\"", 1);
            cryptoHashProcessStr(result, section);
            cryptoHashProcessC(result, (const unsigned char *)"\":{", 3);

            StringList *keyList = iniSectionKeyList(ini, section);
            unsigned int keyListSize = strLstSize(keyList);

            // Loop through values and build the section
            for (unsigned int keyIdx = 0; keyIdx < keyListSize; keyIdx++)
            {
                String *key = strLstGet(keyList, keyIdx);

                // Skip the backrest checksum in the file
                if ((strEq(section, INI_SECTION_BACKREST_STR) && !strEq(key, INI_KEY_CHECKSUM_STR)) ||
                    !strEq(section, INI_SECTION_BACKREST_STR))
                {
                    cryptoHashProcessC(result, (const unsigned char *)"\"", 1);
                    cryptoHashProcessStr(result, key);
                    cryptoHashProcessC(result, (const unsigned char *)"\":", 2);
                    cryptoHashProcessStr(result, varStr(iniGet(ini, section, strLstGet(keyList, keyIdx))));
                    if ((keyListSize > 1) && (keyIdx < keyListSize - 1))
                        cryptoHashProcessC(result, (const unsigned char *)",", 1);
                }
            }

            // Close the key/value list
            cryptoHashProcessC(result, (const unsigned char *)"}", 1);
        }

        // JSON closing bracket
        cryptoHashProcessC(result, (const unsigned char *)"}", 1);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_TEST_RESULT(CRYPTO_HASH, result);
}

/***********************************************************************************************************************************
Load and validate the info file (or copy)
***********************************************************************************************************************************/
static bool
infoLoad(Info *this, const Storage *storage, bool copyFile, CipherType cipherType, const String *cipherPass)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace)
        FUNCTION_DEBUG_PARAM(INFO, this);
        FUNCTION_DEBUG_PARAM(STORAGE, storage);
        FUNCTION_DEBUG_PARAM(BOOL, copyFile);                       // Is this the copy file?
        FUNCTION_DEBUG_PARAM(ENUM, cipherType);
        // cipherPass omitted for security

        FUNCTION_DEBUG_ASSERT(this != NULL);
    FUNCTION_DEBUG_END();

    bool result = false;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        String *fileName = copyFile ? strCat(strDup(this->fileName), INI_COPY_EXT) : this->fileName;

        // Attempt to load the file
        StorageFileRead *infoRead = storageNewReadNP(storage, fileName);

        if (cipherType != cipherTypeNone)
        {
            ioReadFilterGroupSet(
                storageFileReadIo(infoRead),
                ioFilterGroupAdd(
                    ioFilterGroupNew(), cipherBlockFilter(cipherBlockNew(cipherModeDecrypt, cipherType, bufNewStr(cipherPass),
                    NULL))));
        }

        // Load and parse the info file
        Buffer *buffer = storageGetNP(infoRead);
        iniParse(this->ini, strNewBuf(buffer));

        // Make sure the ini is valid by testing the checksum
        String *infoChecksum = varStr(iniGet(this->ini, INI_SECTION_BACKREST_STR, INI_KEY_CHECKSUM_STR));

        CryptoHash *hash = infoHash(this->ini);

        // ??? Temporary hack until get json parser: add quotes around hash before comparing
        if (!strEq(infoChecksum, strQuoteZ(bufHex(cryptoHash(hash)), "\"")))
        {
            // Is the checksum present?
            bool checksumMissing = strSize(infoChecksum) < 3;

            THROW_FMT(
                ChecksumError, "invalid checksum in '%s', expected '%s' but %s%s%s", strPtr(fileName),
                strPtr(bufHex(cryptoHash(hash))), checksumMissing ? "no checksum found" : "found '",
                // ??? Temporary hack until get json parser: remove quotes around hash before displaying in messsage
                checksumMissing ? "" : strPtr(strSubN(infoChecksum, 1, strSize(infoChecksum) - 2)),
                checksumMissing ? "" : "'");
        }

        // Make sure that the format is current, otherwise error
        if (varIntForce(iniGet(this->ini, INI_SECTION_BACKREST_STR, INI_KEY_FORMAT_STR)) != REPOSITORY_FORMAT)
        {
            THROW_FMT(
                FormatError, "invalid format in '%s', expected %d but found %d", strPtr(fileName), REPOSITORY_FORMAT,
                varIntForce(iniGet(this->ini, INI_SECTION_BACKREST_STR, INI_KEY_FORMAT_STR)));
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_DEBUG_RESULT(BOOL, result);
}

/***********************************************************************************************************************************
Load an Info object
// ??? Need loadFile parameter to be able to load from a string.
// ??? Need to handle modified flag, encryption and checksum, initialization, etc.
// ??? The file MUST exist currently, so this is not actually creating the object - rather it is loading it
***********************************************************************************************************************************/
Info *
infoNew(const Storage *storage, const String *fileName, CipherType cipherType, const String *cipherPass)
{
    FUNCTION_DEBUG_BEGIN(logLevelDebug);
        FUNCTION_DEBUG_PARAM(STORAGE, storage);
        FUNCTION_DEBUG_PARAM(STRING, fileName);                     // Full path/filename to load
        FUNCTION_DEBUG_PARAM(ENUM, cipherType);
        // cipherPass omitted for security

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

        // Attempt to load the primary file
        TRY_BEGIN()
        {
            infoLoad(this, storage, false, cipherType, cipherPass);
        }
        CATCH_ANY()
        {
            // On error store the error and try to load the copy
            String *primaryError = strNewFmt("%s: %s", errorTypeName(errorType()), errorMessage());

            TRY_BEGIN()
            {
                infoLoad(this, storage, true, cipherType, cipherPass);
            }
            CATCH_ANY()
            {
                THROW_FMT(
                    FileOpenError, "unable to load info file '%s' or '%s" INI_COPY_EXT "':\n%s\n%s: %s",
                    strPtr(storagePathNP(storage, this->fileName)), strPtr(storagePathNP(storage, this->fileName)),
                    strPtr(primaryError), errorTypeName(errorType()), errorMessage());
            }
            TRY_END();
        }
        TRY_END();

        // Load the cipher passphrase if it exists
        String *cipherPass = varStr(iniGetDefault(this->ini, INI_SECTION_CIPHER_STR, INI_KEY_CIPHER_PASS_STR, NULL));

        if (cipherPass != NULL)
        {
            this->cipherPass = strSubN(cipherPass, 1, strSize(cipherPass) - 2);
            strFree(cipherPass);
        }
    }
    MEM_CONTEXT_NEW_END();

    // Return buffer
    FUNCTION_DEBUG_RESULT(INFO, this);
}

/***********************************************************************************************************************************
Accessor functions
***********************************************************************************************************************************/
const String *
infoCipherPass(const Info *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(INFO, this);

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RESULT(CONST_STRING, this->cipherPass);
}

Ini *
infoIni(const Info *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(INFO, this);

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RESULT(INI, this->ini);
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

/***********************************************************************************************************************************
Free the object
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
