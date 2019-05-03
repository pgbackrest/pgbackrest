/***********************************************************************************************************************************
Info Handler
***********************************************************************************************************************************/
#include "build.auto.h"

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "common/crypto/cipherBlock.h"
#include "common/crypto/hash.h"
#include "common/debug.h"
#include "common/io/filter/filter.intern.h"
#include "common/ini.h"
#include "common/log.h"
#include "common/memContext.h"
#include "common/object.h"
#include "common/type/json.h"
#include "info/info.h"
#include "storage/helper.h"
#include "version.h"

/***********************************************************************************************************************************
Internal constants
***********************************************************************************************************************************/
#define INFO_COPY_EXT                                                ".copy"

STRING_STATIC(INFO_SECTION_BACKREST_STR,                            "backrest");
STRING_STATIC(INFO_SECTION_CIPHER_STR,                              "cipher");

STRING_STATIC(INFO_KEY_CIPHER_PASS_STR,                             "cipher-pass");
STRING_STATIC(INFO_KEY_CHECKSUM_STR,                                "backrest-checksum");
STRING_EXTERN(INFO_KEY_FORMAT_STR,                                  INFO_KEY_FORMAT);
STRING_EXTERN(INFO_KEY_VERSION_STR,                                 INFO_KEY_VERSION);

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct Info
{
    MemContext *memContext;                                         // Mem context
    const String *cipherPass;                                       // Cipher passphrase if set
};

OBJECT_DEFINE_FREE(INFO);

/***********************************************************************************************************************************
Generate hash for the contents of an ini file
***********************************************************************************************************************************/
String *
infoHash(const Ini *ini)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(INI, ini);
    FUNCTION_TEST_END();

    ASSERT(ini != NULL);

    String *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        IoFilter *hash = cryptoHashNew(HASH_TYPE_SHA1_STR);
        StringList *sectionList = strLstSort(iniSectionList(ini), sortOrderAsc);

        // Initial JSON opening bracket
        ioFilterProcessIn(hash, BUFSTRDEF("{"));

        // Loop through sections and create hash for checking checksum
        for (unsigned int sectionIdx = 0; sectionIdx < strLstSize(sectionList); sectionIdx++)
        {
            String *section = strLstGet(sectionList, sectionIdx);

            // Add a comma before additional sections
            if (sectionIdx != 0)
                ioFilterProcessIn(hash, BUFSTRDEF(","));

            // Create the section header
            ioFilterProcessIn(hash, BUFSTRDEF("\""));
            ioFilterProcessIn(hash, BUFSTR(section));
            ioFilterProcessIn(hash, BUFSTRDEF("\":{"));

            StringList *keyList = strLstSort(iniSectionKeyList(ini, section), sortOrderAsc);
            unsigned int keyListSize = strLstSize(keyList);

            // Loop through values and build the section
            for (unsigned int keyIdx = 0; keyIdx < keyListSize; keyIdx++)
            {
                String *key = strLstGet(keyList, keyIdx);

                // Skip the backrest checksum in the file
                if ((strEq(section, INFO_SECTION_BACKREST_STR) && !strEq(key, INFO_KEY_CHECKSUM_STR)) ||
                    !strEq(section, INFO_SECTION_BACKREST_STR))
                {
                    ioFilterProcessIn(hash, BUFSTRDEF("\""));
                    ioFilterProcessIn(hash, BUFSTR(key));
                    ioFilterProcessIn(hash, BUFSTRDEF("\":"));
                    ioFilterProcessIn(hash, BUFSTR(iniGet(ini, section, strLstGet(keyList, keyIdx))));

                    if ((keyListSize > 1) && (keyIdx < keyListSize - 1))
                        ioFilterProcessIn(hash, BUFSTRDEF(","));
                }
            }

            // Close the key/value list
            ioFilterProcessIn(hash, BUFSTRDEF("}"));
        }

        // JSON closing bracket
        ioFilterProcessIn(hash, BUFSTRDEF("}"));

        Variant *resultVar = ioFilterResult(hash);

        memContextSwitch(MEM_CONTEXT_OLD());
        result = strDup(varStr(resultVar));
        memContextSwitch(MEM_CONTEXT_TEMP());
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_TEST_RETURN(result);
}

/***********************************************************************************************************************************
Load and validate the info file (or copy)
***********************************************************************************************************************************/
static Ini *
infoLoad(Info *this, const Storage *storage, const String *fileName, bool copyFile, CipherType cipherType, const String *cipherPass)
{
    FUNCTION_LOG_BEGIN(logLevelTrace)
        FUNCTION_LOG_PARAM(INFO, this);
        FUNCTION_LOG_PARAM(STORAGE, storage);
        FUNCTION_LOG_PARAM(STRING, fileName);                     // Full path/filename to load
        FUNCTION_LOG_PARAM(BOOL, copyFile);                       // Is this the copy file?
        FUNCTION_LOG_PARAM(ENUM, cipherType);
        FUNCTION_TEST_PARAM(STRING, cipherPass);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(fileName != NULL);

    Ini *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        const String *fileNameExt = copyFile ? strNewFmt("%s" INFO_COPY_EXT, strPtr(fileName)) : fileName;

        // Attempt to load the file
        StorageRead *infoRead = storageNewReadNP(storage, fileNameExt);

        if (cipherType != cipherTypeNone)
        {
            ioReadFilterGroupSet(
                storageReadIo(infoRead),
                ioFilterGroupAdd(ioFilterGroupNew(), cipherBlockNew(cipherModeDecrypt, cipherType, BUFSTR(cipherPass), NULL)));
        }

        // Load and parse the info file
        Buffer *buffer = NULL;

        TRY_BEGIN()
        {
            buffer = storageGetNP(infoRead);
        }
        CATCH(CryptoError)
        {
            THROW_FMT(
                CryptoError, "'%s' %s\nHINT: Is or was the repo encrypted?", strPtr(storagePathNP(storage, fileNameExt)),
                errorMessage());
        }
        TRY_END();

        result = iniNew();
        iniParse(result, strNewBuf(buffer));

        // Make sure the ini is valid by testing the checksum
        const String *infoChecksumJson = iniGet(result, INFO_SECTION_BACKREST_STR, INFO_KEY_CHECKSUM_STR);
        const String *checksum = infoHash(result);

        if (strSize(infoChecksumJson) == 0)
        {
            THROW_FMT(
                ChecksumError, "invalid checksum in '%s', expected '%s' but no checksum found",
                strPtr(storagePathNP(storage, fileNameExt)), strPtr(checksum));
        }
        else
        {
            const String *infoChecksum = jsonToStr(infoChecksumJson);

            if (!strEq(infoChecksum, checksum))
            {
                THROW_FMT(
                    ChecksumError, "invalid checksum in '%s', expected '%s' but found '%s'",
                    strPtr(storagePathNP(storage, fileNameExt)), strPtr(checksum), strPtr(infoChecksum));
            }
        }

        // Make sure that the format is current, otherwise error
        if (jsonToUInt(iniGet(result, INFO_SECTION_BACKREST_STR, INFO_KEY_FORMAT_STR)) != REPOSITORY_FORMAT)
        {
            THROW_FMT(
                FormatError, "invalid format in '%s', expected %d but found %d", strPtr(fileNameExt), REPOSITORY_FORMAT,
                varIntForce(VARSTR(iniGet(result, INFO_SECTION_BACKREST_STR, INFO_KEY_FORMAT_STR))));
        }

        iniMove(result, MEM_CONTEXT_OLD());
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(INI, result);
}

/***********************************************************************************************************************************
Create new object
***********************************************************************************************************************************/
Info *
infoNew(void)
{
    FUNCTION_LOG_VOID(logLevelTrace);

    Info *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("Info")
    {
        // Create object
        this = memNew(sizeof(Info));
        this->memContext = MEM_CONTEXT_NEW();
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_LOG_RETURN(INFO, this);
}

/***********************************************************************************************************************************
Create new object and load contents from a file
***********************************************************************************************************************************/
Info *
infoNewLoad(const Storage *storage, const String *fileName, CipherType cipherType, const String *cipherPass, Ini **ini)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STORAGE, storage);
        FUNCTION_LOG_PARAM(STRING, fileName);                     // Full path/filename to load
        FUNCTION_LOG_PARAM(ENUM, cipherType);
        FUNCTION_TEST_PARAM(STRING, cipherPass);
        FUNCTION_LOG_PARAM_P(INI, ini);
    FUNCTION_LOG_END();

    ASSERT(storage != NULL);
    ASSERT(fileName != NULL);
    ASSERT(cipherType == cipherTypeNone || cipherPass != NULL);

    Info *this = infoNew();

    MEM_CONTEXT_BEGIN(this->memContext)
    {
        Ini *iniLocal = NULL;

        // Attempt to load the primary file
        TRY_BEGIN()
        {
            iniLocal = infoLoad(this, storage, fileName, false, cipherType, cipherPass);
        }
        CATCH_ANY()
        {
            // On error store the error and try to load the copy
            String *primaryError = strNewFmt("%s: %s", errorTypeName(errorType()), errorMessage());
            bool primaryMissing = errorType() == &FileMissingError;
            const ErrorType *primaryErrorType = errorType();

            TRY_BEGIN()
            {
                iniLocal = infoLoad(this, storage, fileName, true, cipherType, cipherPass);
            }
            CATCH_ANY()
            {
                // If both copies of the file have the same error then throw that error,
                // else if one file is missing but the other is in error and it is not missing, throw that error
                // else throw an open error
                THROWP_FMT(
                    errorType() == primaryErrorType ? errorType() :
                        (errorType() == &FileMissingError ? primaryErrorType :
                        (primaryMissing ? errorType() : &FileOpenError)),
                    "unable to load info file '%s' or '%s" INFO_COPY_EXT "':\n%s\n%s: %s",
                    strPtr(storagePathNP(storage, fileName)), strPtr(storagePathNP(storage, fileName)),
                    strPtr(primaryError), errorTypeName(errorType()), errorMessage());
            }
            TRY_END();
        }
        TRY_END();

        // Load the cipher passphrase if it exists
        const String *cipherPass = iniGetDefault(iniLocal, INFO_SECTION_CIPHER_STR, INFO_KEY_CIPHER_PASS_STR, NULL);

        if (cipherPass != NULL)
            this->cipherPass = jsonToStr(cipherPass);

        if (ini != NULL)
            *ini = iniMove(iniLocal, MEM_CONTEXT_OLD());
    }
    MEM_CONTEXT_END();

    FUNCTION_LOG_RETURN(INFO, this);
}

/***********************************************************************************************************************************
Save to file
***********************************************************************************************************************************/
void
infoSave(
    Info *this, Ini *ini, const Storage *storage, const String *fileName, CipherType cipherType, const String *cipherPass)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(INFO, this);
        FUNCTION_LOG_PARAM(INI, ini);
        FUNCTION_LOG_PARAM(STORAGE, storage);
        FUNCTION_LOG_PARAM(STRING, fileName);
        FUNCTION_LOG_PARAM(ENUM, cipherType);
        FUNCTION_TEST_PARAM(STRING, cipherPass);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(ini != NULL);
    ASSERT(storage != NULL);
    ASSERT(fileName != NULL);
    ASSERT(cipherType == cipherTypeNone || cipherPass != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Add common info values
        iniSet(ini, INFO_SECTION_BACKREST_STR, INFO_KEY_VERSION_STR, jsonFromStr(STRDEF(PROJECT_VERSION)));
        iniSet(ini, INFO_SECTION_BACKREST_STR, INFO_KEY_FORMAT_STR, jsonFromUInt(REPOSITORY_FORMAT));
        iniSet(ini, INFO_SECTION_BACKREST_STR, INFO_KEY_CHECKSUM_STR, jsonFromStr(infoHash(ini)));

        // Save info file
        IoWrite *infoWrite = storageWriteIo(storageNewWriteNP(storage, fileName));

        if (cipherType != cipherTypeNone)
        {
            ioWriteFilterGroupSet(
                infoWrite,
                ioFilterGroupAdd(ioFilterGroupNew(), cipherBlockNew(cipherModeEncrypt, cipherType, BUFSTR(cipherPass), NULL)));
        }

        iniSave(ini, infoWrite);

        // Copy to .copy file
        storageCopyNP(
            storageNewReadNP(storage, fileName), storageNewWriteNP(storage, strNewFmt("%s" INFO_COPY_EXT, strPtr(fileName))));
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Accessor functions
***********************************************************************************************************************************/
const String *
infoCipherPass(const Info *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(INFO, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->cipherPass);
}
