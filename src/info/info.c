/***********************************************************************************************************************************
Info Handler
***********************************************************************************************************************************/
#include <build.h>

#include <ctype.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "common/crypto/cipherBlock.h"
#include "common/crypto/hash.h"
#include "common/debug.h"
#include "common/format.h"
#include "common/ini.h"
#include "common/io/bufferRead.h"
#include "common/io/bufferWrite.h"
#include "common/io/filter/filter.h"
#include "common/io/io.h"
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
infoNew(const unsigned int format, const CipherSpec *const cipherSpecSub)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(UINT, format);
        FUNCTION_LOG_PARAM(CIPHER_SPEC, cipherSpecSub);
    FUNCTION_LOG_END();

    ASSERT(format >= REPOSITORY_FORMAT_MIN && format <= REPOSITORY_FORMAT_MAX);

    OBJ_NEW_BEGIN(Info, .childQty = MEM_CONTEXT_QTY_MAX)
    {
        *this = (Info){};

        // Cipher used to encrypt/decrypt subsequent dependent files. Value may be NULL.
        infoCipherSpecSet(this, cipherSpecSub);
        this->pub.format = format;
        this->pub.backrestVersion = STRDEF(PROJECT_VERSION);
    }
    OBJ_NEW_END();

    FUNCTION_LOG_RETURN(INFO, this);
}

/**********************************************************************************************************************************/
#define INFO_SECTION_BACKREST                                       "backrest"
#define INFO_KEY_CHECKSUM                                           "backrest-checksum"
#define INFO_SECTION_CIPHER                                         "cipher"
#define INFO_KEY_CIPHER_DIGEST                                      "cipher-digest"
#define INFO_KEY_CIPHER_PASS                                        "cipher-pass"

FN_EXTERN Info *
infoNewLoad(
    IoRead *const read, const CipherSpec *const cipherSpec, InfoLoadNewCallback *const callbackFunction,
    void *const callbackData, const InfoNewLoadParam param)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(IO_READ, read);
        FUNCTION_LOG_PARAM(CIPHER_SPEC, cipherSpec);
        FUNCTION_LOG_PARAM(FUNCTIONP, callbackFunction);
        FUNCTION_LOG_PARAM_P(VOID, callbackData);
        FUNCTION_LOG_PARAM(BOOL, param.header);
    FUNCTION_LOG_END();

    FUNCTION_AUDIT_CALLBACK();

    ASSERT(read != NULL);
    ASSERT(cipherSpec != NULL);
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
            HashType cipherDigest = hashTypeSha1;                               // Digest the stored pass derives with

            INFO_CHECKSUM_BEGIN(checksumActualFilter);

            TRY_BEGIN()
            {
                // The content is decrypted as it is parsed. A file that may contain a header is read with one, which the cipher
                // consumes and reports the format of once the read is done.
                if (cipherSpecType(cipherSpec) != cipherTypeNone)
                {
                    ioFilterGroupAdd(
                        ioReadFilterGroup(read), cipherBlockNewP(cipherModeDecrypt, cipherSpec, .header = param.header));
                }

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
                            // Validate and store format
                            if (strEqZ(value->key, INFO_KEY_FORMAT))
                            {
                                const unsigned int format = jsonReadUInt(jsonReadNew(value->value));
                                repoFormatValidate(format);

                                this->pub.format = format;
                            }
                            // Store pgBackRest version
                            else if (strEqZ(value->key, INFO_KEY_VERSION))
                            {
                                MEM_CONTEXT_OBJ_BEGIN(this)
                                {
                                    this->pub.backrestVersion = varStr(jsonToVar(value->value));
                                }
                                MEM_CONTEXT_OBJ_END();
                            }
                            // Store checksum to be validated later
                            else if (strEqZ(value->key, INFO_KEY_CHECKSUM))
                            {
                                MEM_CONTEXT_OBJ_BEGIN(this)
                                {
                                    checksumExpected = varStr(jsonToVar(value->value));
                                }
                                MEM_CONTEXT_OBJ_END();
                            }
                        }
                        // Process cipher section
                        else if (strEqZ(value->section, INFO_SECTION_CIPHER))
                        {
                            // Store the digest the pass derives with. A file written before the digest was stored has none, so the
                            // default is what every repository derived with then.
                            if (strEqZ(value->key, INFO_KEY_CIPHER_DIGEST))
                            {
                                cipherDigest = jsonReadStrId(jsonReadNew(value->value));
                            }
                            // No validation needed for cipher-pass, just store it
                            else if (strEqZ(value->key, INFO_KEY_CIPHER_PASS))
                            {
                                MEM_CONTEXT_OBJ_BEGIN(this)
                                {
                                    // The dependent files are encrypted with the same cipher type as this one and derive with the
                                    // digest stored with the pass. The digest is read before this since the keys come out in order
                                    // and digest sorts before pass.
                                    this->pub.cipherSpec = cipherSpecNewP(
                                        cipherSpecType(cipherSpec), BUFSTR(varStr(jsonToVar(value->value))),
                                        .digest = cipherDigest);
                                }
                                MEM_CONTEXT_OBJ_END();
                            }
                        }
                        // Else pass to callback for processing
                        else
                            callbackFunction(callbackData, value->section, value->key, jsonReadNew(value->value));

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

            // Verify the checksum first so a file that is empty or not an info file at all is reported as a checksum failure
            // rather than as a missing format
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

            // The format defines how everything else is read, so a file without one is not an info file this version can use. The
            // format is zero until the key is found and the value stored, so if we got here then the key was not found.
            if (infoFormat(this) == 0)
                THROW(FormatError, "repository format not found\nHINT: is this a valid " PROJECT_NAME " info file?");

            // Only a cipher that read a header reports a format, so a result here means the file had one. The header is written
            // from the same format as the content, so a file where they disagree has been damaged or put together from parts of two
            // files.
            PackRead *const cipherResult = ioFilterGroupResultP(ioReadFilterGroup(read), CIPHER_BLOCK_FILTER_TYPE);

            if (cipherResult != NULL)
            {
                const unsigned int formatHeader = cipherBlockFormat(cipherResult);

                if (this->pub.format != formatHeader)
                {
                    THROW_FMT(
                        FormatError, "repository format %u does not match header format %u", this->pub.format, formatHeader);
                }
            }
        }
        MEM_CONTEXT_TEMP_END();

        // A file with no cipher section has no encrypted dependent files
        if (this->pub.cipherSpec == NULL)
            infoCipherSpecSet(this, NULL);
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
        infoSaveValue(&data, INFO_SECTION_BACKREST, INFO_KEY_FORMAT, jsonFromVar(VARUINT(infoFormat(this))));
        infoSaveValue(&data, INFO_SECTION_BACKREST, INFO_KEY_VERSION, jsonFromVar(VARSTRDEF(PROJECT_VERSION)));

        // Add cipher passphrase if defined
        if (cipherSpecType(infoCipherSpec(this)) != cipherTypeNone)
        {
            callbackFunction(callbackData, STRDEF(INFO_SECTION_CIPHER), &data);

            // Store the digest the pass derives with so that a pass outlives the format of the file it is stored in. A pass in a
            // file written before this could be stored derives with SHA-1, which is what a reader assumes when it finds no digest.
            if (infoFormat(this) >= REPOSITORY_FORMAT_6)
            {
                char digestZ[STRID_MAX + 1];
                strIdToZ(cipherSpecDigest(infoCipherSpec(this)), digestZ);

                infoSaveValue(&data, INFO_SECTION_CIPHER, INFO_KEY_CIPHER_DIGEST, jsonFromVar(VARSTRZ(digestZ)));
            }

            infoSaveValue(
                &data, INFO_SECTION_CIPHER, INFO_KEY_CIPHER_PASS,
                jsonFromVar(VARSTR(strNewBuf(cipherSpecPass(infoCipherSpec(this))))));
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
infoFormatSet(Info *const this, const unsigned int format)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(INFO, this);
        FUNCTION_TEST_PARAM(UINT, format);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(format >= REPOSITORY_FORMAT_MIN && format <= REPOSITORY_FORMAT_MAX);

    this->pub.format = format;

    FUNCTION_TEST_RETURN_VOID();
}

FN_EXTERN void
infoCipherSpecSet(Info *const this, const CipherSpec *const cipherSpec)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(INFO, this);
        FUNCTION_TEST_PARAM(CIPHER_SPEC, cipherSpec);
    FUNCTION_TEST_END();

    FUNCTION_AUDIT_IF(memContextCurrent() != objMemContext(this));  // Do not audit calls from within the object

    ASSERT(this != NULL);

    MEM_CONTEXT_OBJ_BEGIN(this)
    {
        // Copy so the caller is free to release what was passed in, and so the getter never returns NULL
        this->pub.cipherSpec = cipherSpec == NULL ? cipherSpecNewNone() : cipherSpecDup(cipherSpec);
    }
    MEM_CONTEXT_OBJ_END();

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
