/***********************************************************************************************************************************
Info Handler
***********************************************************************************************************************************/
#include "build.auto.h"

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "common/type/convert.h"
#include "common/crypto/cipherBlock.h"
#include "common/crypto/hash.h"
#include "common/debug.h"
#include "common/encode.h"
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
#define INFO_SECTION_BACKREST                                       "backrest"
STRING_STATIC(INFO_SECTION_BACKREST_STR,                            INFO_SECTION_BACKREST);
STRING_STATIC(INFO_SECTION_CIPHER_STR,                              "cipher");

STRING_STATIC(INFO_KEY_CIPHER_PASS_STR,                             "cipher-pass");
#define INFO_KEY_CHECKSUM                                           "backrest-checksum"
    STRING_STATIC(INFO_KEY_CHECKSUM_STR,                            INFO_KEY_CHECKSUM);
STRING_EXTERN(INFO_KEY_FORMAT_STR,                                  INFO_KEY_FORMAT);
STRING_EXTERN(INFO_KEY_VERSION_STR,                                 INFO_KEY_VERSION);

/***********************************************************************************************************************************
Object types
***********************************************************************************************************************************/
struct Info
{
    MemContext *memContext;                                         // Mem context
    const String *cipherPass;                                       // Cipher passphrase if set
};

OBJECT_DEFINE_MOVE(INFO);
OBJECT_DEFINE_FREE(INFO);

struct InfoSave
{
    MemContext *memContext;                                         // Mem context
    IoWrite *write;                                                 // Write object
    IoFilter *checksum;                                             // hash to generate file checksum
    String *sectionLast;                                            // The last section seen
};

/***********************************************************************************************************************************
Macros and buffer constants for checksum generation
***********************************************************************************************************************************/
#define INFO_CHECKSUM_BEGIN(checksum)                                                                                              \
    do                                                                                                                             \
    {                                                                                                                              \
        ioFilterProcessIn(checksum, BRACEL_BUF);                                                                                   \
    }                                                                                                                              \
    while (0)

BUFFER_STRDEF_STATIC(INFO_CHECKSUM_SECTION_END_BUF, "\":{");

#define INFO_CHECKSUM_SECTION(checksum, section)                                                                                   \
    do                                                                                                                             \
    {                                                                                                                              \
        ioFilterProcessIn(checksum, QUOTED_BUF);                                                                                   \
        ioFilterProcessIn(checksum, BUFSTR(section));                                                                              \
        ioFilterProcessIn(checksum, INFO_CHECKSUM_SECTION_END_BUF);                                                                \
    }                                                                                                                              \
    while (0)

BUFFER_STRDEF_STATIC(INFO_CHECKSUM_SECTION_NEXT_END_BUF, "},");

#define INFO_CHECKSUM_SECTION_NEXT(checksum)                                                                                       \
    do                                                                                                                             \
    {                                                                                                                              \
        ioFilterProcessIn(checksum, INFO_CHECKSUM_SECTION_NEXT_END_BUF);                                                           \
    }                                                                                                                              \
    while (0)

BUFFER_STRDEF_STATIC(INFO_CHECKSUM_KEY_VALUE_END_BUF, "\":");

#define INFO_CHECKSUM_KEY_VALUE(checksum, key, value)                                                                              \
    do                                                                                                                             \
    {                                                                                                                              \
        ioFilterProcessIn(checksum, QUOTED_BUF);                                                                                   \
        ioFilterProcessIn(checksum, BUFSTR(key));                                                                                  \
        ioFilterProcessIn(checksum, INFO_CHECKSUM_KEY_VALUE_END_BUF);                                                              \
        ioFilterProcessIn(checksum, BUFSTR(value));                                                                                \
    }                                                                                                                              \
    while (0)

#define INFO_CHECKSUM_KEY_VALUE_NEXT(checksum)                                                                                     \
    do                                                                                                                             \
    {                                                                                                                              \
        ioFilterProcessIn(checksum, COMMA_BUF);                                                                                    \
    }                                                                                                                              \
    while (0)

BUFFER_STRDEF_STATIC(INFO_CHECKSUM_END_BUF, "}}");

#define INFO_CHECKSUM_END(checksum)                                                                                                \
    do                                                                                                                             \
    {                                                                                                                              \
        ioFilterProcessIn(checksum, INFO_CHECKSUM_END_BUF);                                                                        \
    }                                                                                                                              \
    while (0)

/***********************************************************************************************************************************
Internal constructor
***********************************************************************************************************************************/
static Info *
infoNewInternal(void)
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
Create new object
***********************************************************************************************************************************/
Info *
infoNew(CipherType cipherType, const String *cipherPassSub)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(ENUM, cipherType);
        FUNCTION_TEST_PARAM(STRING, cipherPassSub);                 // Use FUNCTION_TEST so cipher is not logged
    FUNCTION_LOG_END();

    // Ensure cipherPassSub is set and not an empty string when cipherType is not NONE and is not set when cipherType is NONE
    ASSERT(
        !((cipherType == cipherTypeNone && cipherPassSub != NULL) ||
          (cipherType != cipherTypeNone && (cipherPassSub == NULL || strSize(cipherPassSub) == 0))));

    Info *this = infoNewInternal();

    // Cipher used to encrypt/decrypt subsequent dependent files. Value may be NULL.
    if (cipherPassSub != NULL)
        this->cipherPass = cipherPassSub;

    FUNCTION_LOG_RETURN(INFO, this);
}

/***********************************************************************************************************************************
Load and validate the info file (or copy)
***********************************************************************************************************************************/
typedef struct InfoLoadData
{
    MemContext *memContext;                                         // Mem context to use for storing data in this structure
    InfoLoadNewCallback *callbackFunction;                          // Callback function for child object
    void *callbackData;                                             // Callback data for child object
    Info *info;                                                     // Info object
    String *sectionLast;                                            // The last section seen during load
    IoFilter *checksumActual;                                       // Checksum calculated from the file
    String *checksumExpected;                                       // Checksum found in ini file
} InfoLoadData;

static void
infoLoadCallback(void *callbackData, const String *section, const String *key, const String *value)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM_P(VOID, callbackData);
        FUNCTION_TEST_PARAM(STRING, section);
        FUNCTION_TEST_PARAM(STRING, key);
        FUNCTION_TEST_PARAM(STRING, value);
    FUNCTION_TEST_END();

    ASSERT(callbackData != NULL);
    ASSERT(section != NULL);
    ASSERT(key != NULL);
    ASSERT(value != NULL);

    InfoLoadData *data = (InfoLoadData *)callbackData;

    // Calculate checksum
    if (!(strEq(section, INFO_SECTION_BACKREST_STR) && strEq(key, INFO_KEY_CHECKSUM_STR)))
    {
        if (data->sectionLast == NULL || !strEq(section, data->sectionLast))
        {
            if (data->sectionLast != NULL)
                INFO_CHECKSUM_SECTION_NEXT(data->checksumActual);

            INFO_CHECKSUM_SECTION(data->checksumActual, section);

            MEM_CONTEXT_BEGIN(data->memContext)
            {
                data->sectionLast = strDup(section);
            }
            MEM_CONTEXT_END();
        }
        else
            INFO_CHECKSUM_KEY_VALUE_NEXT(data->checksumActual);

        INFO_CHECKSUM_KEY_VALUE(data->checksumActual, key, value);
    }

    // Process backrest section
    if (strEq(section, INFO_SECTION_BACKREST_STR))
    {
        // Validate format
        if (strEq(key, INFO_KEY_FORMAT_STR))
        {
            if (jsonToUInt(value) != REPOSITORY_FORMAT)
                THROW_FMT(FormatError, "expected format %d but found %d", REPOSITORY_FORMAT, cvtZToInt(strPtr(value)));
        }
        // Store checksum to be validated later
        else if (strEq(key, INFO_KEY_CHECKSUM_STR))
        {
            MEM_CONTEXT_BEGIN(data->memContext)
            {
                data->checksumExpected = jsonToStr(value);
            }
            MEM_CONTEXT_END();
        }
    }
    // Process cipher section
    else if (strEq(section, INFO_SECTION_CIPHER_STR))
    {
        // No validation needed for cipher-pass, just store it
        if (strEq(key, INFO_KEY_CIPHER_PASS_STR))
        {
            MEM_CONTEXT_BEGIN(data->info->memContext)
            {
                data->info->cipherPass = jsonToStr(value);
            }
            MEM_CONTEXT_END();
        }
    }
    // Else pass to callback for processing
    else
        data->callbackFunction(infoCallbackTypeValue, data->callbackData, section, key, value);

    FUNCTION_TEST_RETURN_VOID();
}

/***********************************************************************************************************************************
Create new object and load contents from a file
***********************************************************************************************************************************/
Info *
infoNewLoad(IoRead *read, InfoLoadNewCallback *callbackFunction, void *callbackData)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(IO_READ, read);
        FUNCTION_LOG_PARAM(FUNCTIONP, callbackFunction);
        FUNCTION_LOG_PARAM_P(VOID, callbackData);
    FUNCTION_LOG_END();

    ASSERT(read != NULL);
    ASSERT(callbackFunction != NULL);
    ASSERT(callbackData != NULL);

    Info *this = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        this = infoNewInternal();

        InfoLoadData data =
        {
            .memContext = MEM_CONTEXT_TEMP(),
            .callbackFunction = callbackFunction,
            .callbackData = callbackData,
            .info = this,
            .checksumActual = cryptoHashNew(HASH_TYPE_SHA1_STR),
        };

        // Load and parse the info file
        INFO_CHECKSUM_BEGIN(data.checksumActual);

        TRY_BEGIN()
        {
            iniLoad(read, infoLoadCallback, &data);
        }
        CATCH(CryptoError)
        {
            THROW_FMT(CryptoError, "%s\nHINT: is or was the repo encrypted?", errorMessage());
        }
        TRY_END();

        INFO_CHECKSUM_END(data.checksumActual);

        // Verify the checksum
        const String *checksumActual = varStr(ioFilterResult(data.checksumActual));

        if (data.checksumExpected == NULL)
            THROW_FMT(ChecksumError, "invalid checksum, actual '%s' but no checksum found", strPtr(checksumActual));
        else if (!strEq(data.checksumExpected, checksumActual))
        {
            THROW_FMT(
                ChecksumError, "invalid checksum, actual '%s' but expected '%s'", strPtr(checksumActual),
                strPtr(data.checksumExpected));
        }

        infoMove(this, MEM_CONTEXT_OLD());
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(INFO, this);
}

/***********************************************************************************************************************************
Check if the section should be saved
***********************************************************************************************************************************/
bool
infoSaveSection(InfoSave *infoSaveData, const String *section, const String *sectionNext)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(INFO_SAVE, infoSaveData);
        FUNCTION_TEST_PARAM(STRING, section);
        FUNCTION_TEST_PARAM(STRING, sectionNext);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RETURN(
        (infoSaveData->sectionLast == NULL || strCmp(section, infoSaveData->sectionLast) > 0) &&
            (sectionNext == NULL || strCmp(section, sectionNext) < 0));
}

/***********************************************************************************************************************************
Save a value and update checksum
***********************************************************************************************************************************/
void
infoSaveValue(InfoSave *infoSaveData, const String *section, const String *key, const String *value)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(INFO_SAVE, infoSaveData);
        FUNCTION_TEST_PARAM(STRING, section);
        FUNCTION_TEST_PARAM(STRING, key);
        FUNCTION_TEST_PARAM(STRING, value);
    FUNCTION_TEST_END();

    ASSERT(infoSaveData != NULL);
    ASSERT(section != NULL);
    ASSERT(key != NULL);
    ASSERT(value != NULL);

    // Save section
    if (infoSaveData->sectionLast == NULL || !strEq(section, infoSaveData->sectionLast))
    {
        if (infoSaveData->sectionLast != NULL)
        {
            INFO_CHECKSUM_SECTION_NEXT(infoSaveData->checksum);
            ioWriteLine(infoSaveData->write, BUFSTRDEF(""));
        }

        INFO_CHECKSUM_SECTION(infoSaveData->checksum, section);

        ioWrite(infoSaveData->write, BRACKETL_BUF);
        ioWrite(infoSaveData->write, BUFSTR(section));
        ioWriteLine(infoSaveData->write, BRACKETR_BUF);

        MEM_CONTEXT_BEGIN(infoSaveData->memContext)
        {
            infoSaveData->sectionLast = strDup(section);
        }
        MEM_CONTEXT_END();
    }
    else
        INFO_CHECKSUM_KEY_VALUE_NEXT(infoSaveData->checksum);

    // Save key/value
    INFO_CHECKSUM_KEY_VALUE(infoSaveData->checksum, key, value);

    ioWrite(infoSaveData->write, BUFSTR(key));
    ioWrite(infoSaveData->write, EQ_BUF);
    ioWriteLine(infoSaveData->write, BUFSTR(value));

    FUNCTION_TEST_RETURN_VOID();
}

/***********************************************************************************************************************************
Save to file
***********************************************************************************************************************************/
void
infoSave(Info *this, IoWrite *write, InfoSaveCallback callbackFunction, void *callbackData)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(INFO, this);
        FUNCTION_LOG_PARAM(IO_WRITE, write);
        FUNCTION_LOG_PARAM(FUNCTIONP, callbackFunction);
        FUNCTION_LOG_PARAM_P(VOID, callbackData);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(write != NULL);
    ASSERT(callbackFunction != NULL);
    ASSERT(callbackData != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        InfoSave data =
        {
            .memContext = MEM_CONTEXT_TEMP(),
            .write = write,
        };

        ioWriteOpen(data.write);

        // Begin checksum calculation
        data.checksum = cryptoHashNew(HASH_TYPE_SHA1_STR);
        INFO_CHECKSUM_BEGIN(data.checksum);

        // Add version and format
        callbackFunction(callbackData, INFO_SECTION_BACKREST_STR, &data);
        infoSaveValue(&data, INFO_SECTION_BACKREST_STR, INFO_KEY_FORMAT_STR, jsonFromUInt(REPOSITORY_FORMAT));
        infoSaveValue(&data, INFO_SECTION_BACKREST_STR, INFO_KEY_VERSION_STR, jsonFromStr(STRDEF(PROJECT_VERSION)));

        // Add cipher passphrase if defined
        if (this->cipherPass != NULL)
        {
            callbackFunction(callbackData, INFO_SECTION_CIPHER_STR, &data);
            infoSaveValue(&data, INFO_SECTION_CIPHER_STR, INFO_KEY_CIPHER_PASS_STR, jsonFromStr(this->cipherPass));
        }

        // Flush out any additional sections
        callbackFunction(callbackData, NULL, &data);

        // Add checksum (this must be set after all other values or it will not be valid)
        INFO_CHECKSUM_END(data.checksum);

        ioWrite(data.write, BUFSTRDEF("\n[" INFO_SECTION_BACKREST "]\n" INFO_KEY_CHECKSUM "="));
        ioWriteLine(data.write, BUFSTR(jsonFromVar(ioFilterResult(data.checksum), 0)));

        // Close the file
        ioWriteClose(data.write);
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

/***********************************************************************************************************************************
Load info file(s)
***********************************************************************************************************************************/
// Info *
// infoLoad(CipherType cipherType, const String *cipherPass,

            // // Attempt to load the primary file
            // TRY_BEGIN()
            // {
            //     infoLoad(this, storage, fileName, false, cipherType, cipherPass, callbackFunction, callbackData);
            // }
            // CATCH_ANY()
            // {
            //     infoFree(this);
            //     this = infoNewInternal();
            //
            //     // On error store the error and try to load the copy
            //     String *primaryError = strNewFmt("%s: %s", errorTypeName(errorType()), errorMessage());
            //     bool primaryMissing = errorType() == &FileMissingError;
            //     const ErrorType *primaryErrorType = errorType();
            //
            //     // Notify callback of a reset
            //     callbackFunction(infoCallbackTypeReset, callbackData, NULL, NULL, NULL);
            //
            //     TRY_BEGIN()
            //     {
            //         infoLoad(this, storage, fileName, true, cipherType, cipherPass, callbackFunction, callbackData);
            //     }
            //     CATCH_ANY()
            //     {
            //         // If both copies of the file have the same error then throw that error,
            //         // else if one file is missing but the other is in error and it is not missing, throw that error
            //         // else throw an open error
            //         THROWP_FMT(
            //             errorType() == primaryErrorType ? errorType() :
            //                 (errorType() == &FileMissingError ? primaryErrorType :
            //                 (primaryMissing ? errorType() : &FileOpenError)),
            //             "unable to load info file '%s' or '%s" INFO_COPY_EXT "':\n%s\n%s: %s",
            //             strPtr(storagePathNP(storage, fileName)), strPtr(storagePathNP(storage, fileName)),
            //             strPtr(primaryError), errorTypeName(errorType()), errorMessage());
            //     }
            //     TRY_END();
            // }
            // TRY_END();
