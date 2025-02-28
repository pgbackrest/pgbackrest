/***********************************************************************************************************************************
Info Handler
***********************************************************************************************************************************/
#include "build.auto.h"

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "common/crypto/hash.h"
#include "common/debug.h"
#include "common/ini.h"
#include "common/io/filter/filter.h"
#include "common/log.h"
#include "common/type/convert.h"
#include "common/type/json.h"
#include "common/type/object.h"
#include "info/info.h"
#include "storage/helper.h"
#include "version.h"

/***********************************************************************************************************************************
Object types
***********************************************************************************************************************************/
struct Info
{
    InfoPub pub;                                                    // Publicly accessible variables
};

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

BUFFER_STRDEF_STATIC(INFO_CHECKSUM_KEY_VALUE_END_BUF, ":");

#define INFO_CHECKSUM_KEY_VALUE(checksum, key, value)                                                                              \
    do                                                                                                                             \
    {                                                                                                                              \
        ioFilterProcessIn(checksum, BUFSTR(jsonFromVar(VARSTR(key))));                                                             \
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

/**********************************************************************************************************************************/
FN_EXTERN Info *
infoNew(const String *const cipherPass)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_TEST_PARAM(STRING, cipherPass);                 // Use FUNCTION_TEST so cipher is not logged
    FUNCTION_LOG_END();

    OBJ_NEW_BEGIN(Info, .childQty = MEM_CONTEXT_QTY_MAX)
    {
        *this = (Info){};

        // Cipher used to encrypt/decrypt subsequent dependent files. Value may be NULL.
        infoCipherPassSet(this, cipherPass);
        this->pub.backrestVersion = STRDEF(PROJECT_VERSION);
    }
    OBJ_NEW_END();

    FUNCTION_LOG_RETURN(INFO, this);
}

/**********************************************************************************************************************************/
#define INFO_SECTION_BACKREST                                       "backrest"
#define INFO_KEY_CHECKSUM                                           "backrest-checksum"
#define INFO_SECTION_CIPHER                                         "cipher"
#define INFO_KEY_CIPHER_PASS                                        "cipher-pass"

FN_EXTERN Info *
infoNewLoad(IoRead *const read, InfoLoadNewCallback *const callbackFunction, void *const callbackData)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(IO_READ, read);
        FUNCTION_LOG_PARAM(FUNCTIONP, callbackFunction);
        FUNCTION_LOG_PARAM_P(VOID, callbackData);
    FUNCTION_LOG_END();

    FUNCTION_AUDIT_CALLBACK();

    ASSERT(read != NULL);
    ASSERT(callbackFunction != NULL);
    ASSERT(callbackData != NULL);

    OBJ_NEW_BEGIN(Info, .childQty = MEM_CONTEXT_QTY_MAX)
    {
        *this = (Info){};

        MEM_CONTEXT_TEMP_BEGIN()
        {
            String *const sectionLast = strNew();                               // The last section seen during load
            IoFilter *const checksumActualFilter = cryptoHashNew(hashTypeSha1); // Checksum calculated from the file
            const String *checksumExpected = NULL;                              // Checksum found in ini file

            INFO_CHECKSUM_BEGIN(checksumActualFilter);

            TRY_BEGIN()
            {
                Ini *const ini = iniNewP(read, .strict = true);

                MEM_CONTEXT_TEMP_RESET_BEGIN()
                {
                    const IniValue *value = iniValueNext(ini);

                    while (value != NULL)
                    {
                        // Calculate checksum
                        if (!(strEqZ(value->section, INFO_SECTION_BACKREST) && strEqZ(value->key, INFO_KEY_CHECKSUM)))
                        {
                            if (strEmpty(sectionLast) || !strEq(value->section, sectionLast))
                            {
                                if (!strEmpty(sectionLast))
                                    INFO_CHECKSUM_SECTION_NEXT(checksumActualFilter);

                                INFO_CHECKSUM_SECTION(checksumActualFilter, value->section);
                                strCat(strTrunc(sectionLast), value->section);
                            }
                            else
                                INFO_CHECKSUM_KEY_VALUE_NEXT(checksumActualFilter);

                            INFO_CHECKSUM_KEY_VALUE(checksumActualFilter, value->key, value->value);
                        }

                        // Process backrest section
                        if (strEqZ(value->section, INFO_SECTION_BACKREST))
                        {
                            // Validate format
                            if (strEqZ(value->key, INFO_KEY_FORMAT))
                            {
                                if (varUInt64(jsonToVar(value->value)) != REPOSITORY_FORMAT)
                                {
                                    THROW_FMT(
                                        FormatError, "expected format %d but found %" PRIu64, REPOSITORY_FORMAT,
                                        varUInt64(jsonToVar(value->value)));
                                }
                            }
                            // Store pgBackRest version
                            else if (strEqZ(value->key, INFO_KEY_VERSION))
                            {
                                MEM_CONTEXT_OBJ_BEGIN(this)
                                {
                                    this->pub.backrestVersion = varStr(jsonToVar(value->value));
                                }
                                MEM_CONTEXT_END();
                            }
                            // Store checksum to be validated later
                            else if (strEqZ(value->key, INFO_KEY_CHECKSUM))
                            {
                                MEM_CONTEXT_OBJ_BEGIN(this)
                                {
                                    checksumExpected = varStr(jsonToVar(value->value));
                                }
                                MEM_CONTEXT_END();
                            }
                        }
                        // Process cipher section
                        else if (strEqZ(value->section, INFO_SECTION_CIPHER))
                        {
                            // No validation needed for cipher-pass, just store it
                            if (strEqZ(value->key, INFO_KEY_CIPHER_PASS))
                            {
                                MEM_CONTEXT_OBJ_BEGIN(this)
                                {
                                    this->pub.cipherPass = varStr(jsonToVar(value->value));
                                }
                                MEM_CONTEXT_END();
                            }
                        }
                        // Else pass to callback for processing
                        else
                            callbackFunction(callbackData, value->section, value->key, value->value);

                        value = iniValueNext(ini);
                        MEM_CONTEXT_TEMP_RESET(1000);
                    }
                }
                MEM_CONTEXT_TEMP_END();
            }
            CATCH(CryptoError)
            {
                THROW_FMT(CryptoError, "%s\nHINT: is or was the repo encrypted?", errorMessage());
            }
            TRY_END();

            INFO_CHECKSUM_END(checksumActualFilter);

            // Verify the checksum
            const String *const checksumActual = strNewEncode(
                encodingHex, pckReadBinP(pckReadNew(ioFilterResult(checksumActualFilter))));

            if (checksumExpected == NULL)
                THROW_FMT(ChecksumError, "invalid checksum, actual '%s' but no checksum found", strZ(checksumActual));
            else if (!strEq(checksumExpected, checksumActual))
            {
                THROW_FMT(
                    ChecksumError, "invalid checksum, actual '%s' but expected '%s'", strZ(checksumActual),
                    strZ(checksumExpected));
            }
        }
        MEM_CONTEXT_TEMP_END();
    }
    OBJ_NEW_END();

    FUNCTION_LOG_RETURN(INFO, this);
}

/**********************************************************************************************************************************/
FN_EXTERN bool
infoSaveSection(InfoSave *const infoSaveData, const char *const section, const String *const sectionNext)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(INFO_SAVE, infoSaveData);
        FUNCTION_TEST_PARAM(STRINGZ, section);
        FUNCTION_TEST_PARAM(STRING, sectionNext);
    FUNCTION_TEST_END();

    ASSERT(infoSaveData != NULL);
    ASSERT(section != NULL);

    FUNCTION_TEST_RETURN(
        BOOL,
        (infoSaveData->sectionLast == NULL || strCmpZ(infoSaveData->sectionLast, section) < 0) &&
        (sectionNext == NULL || strCmpZ(sectionNext, section) > 0));
}

/**********************************************************************************************************************************/
FN_EXTERN void
infoSaveValue(InfoSave *const infoSaveData, const char *const section, const char *const key, const String *const jsonValue)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(INFO_SAVE, infoSaveData);
        FUNCTION_TEST_PARAM(STRINGZ, section);
        FUNCTION_TEST_PARAM(STRINGZ, key);
        FUNCTION_TEST_PARAM(STRING, jsonValue);
    FUNCTION_TEST_END();

    FUNCTION_AUDIT_CALLBACK();

    ASSERT(infoSaveData != NULL);
    ASSERT(section != NULL);
    ASSERT(key != NULL);
    ASSERT(jsonValue != NULL);
    ASSERT(strSize(jsonValue) != 0);
    // The JSON value must not be an array because this may be confused with a section in the ini file
    ASSERT(strZ(jsonValue)[0] != '[');

    // Save section
    if (infoSaveData->sectionLast == NULL || !strEqZ(infoSaveData->sectionLast, section))
    {
        if (infoSaveData->sectionLast != NULL)
        {
            INFO_CHECKSUM_SECTION_NEXT(infoSaveData->checksum);
            ioWriteLine(infoSaveData->write, BUFSTRDEF(""));
        }

        INFO_CHECKSUM_SECTION(infoSaveData->checksum, STR(section));

        ioWrite(infoSaveData->write, BRACKETL_BUF);
        ioWrite(infoSaveData->write, BUFSTRZ(section));
        ioWriteLine(infoSaveData->write, BRACKETR_BUF);

        MEM_CONTEXT_BEGIN(infoSaveData->memContext)
        {
            infoSaveData->sectionLast = strNewZ(section);
        }
        MEM_CONTEXT_END();
    }
    else
        INFO_CHECKSUM_KEY_VALUE_NEXT(infoSaveData->checksum);

    // Save key/value
    INFO_CHECKSUM_KEY_VALUE(infoSaveData->checksum, STR(key), jsonValue);

    ioWrite(infoSaveData->write, BUFSTRZ(key));
    ioWrite(infoSaveData->write, EQ_BUF);
    ioWriteLine(infoSaveData->write, BUFSTR(jsonValue));

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
FN_EXTERN void
infoSave(Info *const this, IoWrite *const write, InfoSaveCallback *const callbackFunction, void *const callbackData)
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
        data.checksum = cryptoHashNew(hashTypeSha1);
        INFO_CHECKSUM_BEGIN(data.checksum);

        // Add version and format
        callbackFunction(callbackData, STRDEF(INFO_SECTION_BACKREST), &data);
        infoSaveValue(&data, INFO_SECTION_BACKREST, INFO_KEY_FORMAT, jsonFromVar(VARUINT(REPOSITORY_FORMAT)));
        infoSaveValue(&data, INFO_SECTION_BACKREST, INFO_KEY_VERSION, jsonFromVar(VARSTRDEF(PROJECT_VERSION)));

        // Add cipher passphrase if defined
        if (infoCipherPass(this) != NULL)
        {
            callbackFunction(callbackData, STRDEF(INFO_SECTION_CIPHER), &data);
            infoSaveValue(&data, INFO_SECTION_CIPHER, INFO_KEY_CIPHER_PASS, jsonFromVar(VARSTR(infoCipherPass(this))));
        }

        // Flush out any additional sections
        callbackFunction(callbackData, NULL, &data);

        // Add checksum (this must be set after all other values or it will not be valid)
        INFO_CHECKSUM_END(data.checksum);

        ioWrite(data.write, BUFSTRDEF("\n[" INFO_SECTION_BACKREST "]\n" INFO_KEY_CHECKSUM "="));
        ioWriteLine(
            data.write,
            BUFSTR(jsonFromVar(VARSTR(strNewEncode(encodingHex, pckReadBinP(pckReadNew(ioFilterResult(data.checksum))))))));

        // Close the file
        ioWriteClose(data.write);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Getters/Setters
***********************************************************************************************************************************/
FN_EXTERN void
infoCipherPassSet(Info *const this, const String *const cipherPass)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(INFO, this);
        FUNCTION_TEST_PARAM(STRING, cipherPass);
    FUNCTION_TEST_END();

    FUNCTION_AUDIT_IF(memContextCurrent() != objMemContext(this));  // Do not audit calls from within the object

    ASSERT(this != NULL);

    MEM_CONTEXT_OBJ_BEGIN(this)
    {
        this->pub.cipherPass = strDup(cipherPass);
    }
    MEM_CONTEXT_END();

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
FN_EXTERN void
infoLoad(const String *const error, InfoLoadCallback *const callbackFunction, void *const callbackData)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STRING, error);
        FUNCTION_LOG_PARAM(FUNCTIONP, callbackFunction);
        FUNCTION_LOG_PARAM_P(VOID, callbackData);
    FUNCTION_LOG_END();

    ASSERT(error != NULL);
    ASSERT(callbackFunction != NULL);
    ASSERT(callbackData != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        unsigned int try = 0;
        volatile bool done = false;                                 // Are all files tried? Must be preserved even on error.
        volatile bool loaded = false;                               // Was a file loaded? Must be preserved even on error.
        const ErrorType *loadErrorType = NULL;
        String *loadErrorMessage = NULL;

        do
        {
            // Attempt to load the file
            TRY_BEGIN()
            {
                loaded = callbackFunction(callbackData, try);
                done = true;

                CHECK(AssertError, loaded || try > 0, "file load must be attempted");
            }
            CATCH_ANY()
            {
                // Set error type if none has been set
                if (loadErrorType == NULL)
                {
                    loadErrorType = errorType();
                    loadErrorMessage = strCatFmt(strNew(), "%s:", strZ(error));
                }
                // Else if the error type is different
                else if (loadErrorType != errorType())
                {
                    // Set type that is not file missing (which is likely the most common error)
                    if (loadErrorType == &FileMissingError)
                    {
                        loadErrorType = errorType();
                    }
                    // Else set a generic error
                    else if (errorType() != &FileMissingError)
                        loadErrorType = &FileOpenError;
                }

                // Append new error
                strCatFmt(loadErrorMessage, "\n%s: %s", errorTypeName(errorType()), errorMessage());

                // Try again
                try++;
            }
            TRY_END();
        }
        while (!done);

        // Error when no file was loaded
        if (!loaded)
            THROWP(loadErrorType, strZ(loadErrorMessage));
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}
