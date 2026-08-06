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

/***********************************************************************************************************************************
Header

An encrypted info file at repository format 6 or above begins with a fixed-size plaintext header naming the format it was written
with. The header exists because the digest the passphrase derives with follows the format, and the format is recorded inside the
file that the passphrase encrypts. A reader that could not see the format in advance would have to decrypt to learn what it should
have decrypted with.

A file with no header was written at format 5, the only format there was before the header, so an unrecognized start is not an
error.

The header takes the place of the salted magic that the cipher writes, which is why a file is always encrypted raw. Both are eight
bytes followed by the salt, so the eight bytes are consumed either way and what follows begins with the salt no matter which was
there. It also means a file of either format is opened with the openssl command-line tool the same way: replace the first eight
bytes with the magic that tool expects.

Only encrypted files carry a header. An unencrypted file is parsed straight away and its format is read from the content like any
other value, so a header would tell the reader nothing it does not already have and would change what a user sees in a file they
can read.

Of the four bytes after the magic, the first three are the format and the last is held back for whatever the header turns out to
need. The format comes first so that it is always at the same place, which is what lets a version work out whether it can read the
file at all. Only once the format turns out to be one this version knows is the spare byte examined, and then it must be the
underscore this version writes.

The header is not part of the file content. Whatever writes an info file adds it and whatever reads one consumes it, so nothing
downstream sees anything but the info file itself.
***********************************************************************************************************************************/
#define INFO_HEADER_MAGIC                                           "PGBR"
#define INFO_HEADER_MAGIC_SIZE                                      (sizeof(INFO_HEADER_MAGIC) - 1)
#define INFO_HEADER_RESERVED                                        '_'
#define INFO_HEADER_FORMAT_SIZE                                     3
#define INFO_HEADER_SIZE                                            (INFO_HEADER_MAGIC_SIZE + INFO_HEADER_FORMAT_SIZE + 1)

// Error when the format cannot be read by this version. Called for the format in the header before anything is decrypted and again
// for the format in the content, since the two are written together but stored apart.
static void
infoFormatValidate(const uint64_t format)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(UINT64, format);
    FUNCTION_TEST_END();

    // A format newer than this version can read requires an upgrade. Do not suggest a version since this version cannot know which
    // version added the format.
    if (format > REPOSITORY_FORMAT_MAX)
    {
        THROW_FMT(
            FormatError,
            "repository format %" PRIu64 " requires a newer version of " PROJECT_NAME "\n"
            "HINT: " PROJECT_NAME " " PROJECT_VERSION " supports repository format %d to %d.",
            format, REPOSITORY_FORMAT_MIN, REPOSITORY_FORMAT_MAX);
    }

    // A format older than this version can read requires an older version to migrate the repository
    if (format < REPOSITORY_FORMAT_MIN)
    {
        THROW_FMT(
            FormatError,
            "repository format %" PRIu64 " is no longer supported by " PROJECT_NAME "\n"
            "HINT: " PROJECT_NAME " " PROJECT_VERSION " supports repository format %d to %d.",
            format, REPOSITORY_FORMAT_MIN, REPOSITORY_FORMAT_MAX);
    }

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
FN_EXTERN HashType
infoFormatDigest(const unsigned int format)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(UINT, format);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RETURN(STRING_ID, format >= REPOSITORY_FORMAT_6 ? hashTypeSha256 : hashTypeSha1);
}

// Cipher spec to read or write an info file at a format with. Only the digest differs from the spec the caller supplied, which
// carries the passphrase the repository was configured with.
static CipherSpec *
infoFormatCipherSpec(const unsigned int format, const CipherSpec *const cipherSpec)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(UINT, format);
        FUNCTION_TEST_PARAM(CIPHER_SPEC, cipherSpec);
    FUNCTION_TEST_END();

    ASSERT(cipherSpec != NULL);
    ASSERT(cipherSpecType(cipherSpec) != cipherTypeNone);

    FUNCTION_TEST_RETURN(
        CIPHER_SPEC,
        cipherSpecNewP(cipherSpecType(cipherSpec), cipherSpecPass(cipherSpec), .digest = infoFormatDigest(format)));
}

/***********************************************************************************************************************************
Read the header of an encrypted info file and return a read of the content behind it

The header has to be read before the digest is known, which means the read has been opened and an encryption filter can no longer
be added to it. The content is therefore decrypted into a buffer here rather than as it is parsed. An info file is small enough for
that to be reasonable, and is already built whole in a buffer when it is saved so that it can be written twice.

The format the header gives is returned so that the content can be checked against it.
***********************************************************************************************************************************/
static IoRead *
infoContentRead(IoRead *const read, const CipherSpec *const cipherSpec, unsigned int *const format)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(IO_READ, read);
        FUNCTION_LOG_PARAM(CIPHER_SPEC, cipherSpec);
        FUNCTION_LOG_PARAM_P(UINT, format);
    FUNCTION_LOG_END();

    FUNCTION_AUDIT_HELPER();

    ASSERT(read != NULL);
    ASSERT(cipherSpec != NULL);
    ASSERT(cipherSpecType(cipherSpec) != cipherTypeNone);
    ASSERT(format != NULL);

    // Read what a header would be. A file that has none was written at format 5, where these are the magic the cipher writes, so
    // either way they are consumed and what follows begins with the salt.
    ioReadOpen(read);

    Buffer *const header = bufNew(INFO_HEADER_SIZE);
    ioRead(read, header);

    *format = REPOSITORY_FORMAT_5;

    if (bufUsed(header) == INFO_HEADER_SIZE && memcmp(bufPtrConst(header), INFO_HEADER_MAGIC, INFO_HEADER_MAGIC_SIZE) == 0)
    {
        const char *const headerZ = (const char *)bufPtrConst(header);

        for (unsigned int digitIdx = 0; digitIdx < INFO_HEADER_FORMAT_SIZE; digitIdx++)
        {
            if (!isdigit((unsigned char)headerZ[INFO_HEADER_MAGIC_SIZE + digitIdx]))
                THROW(FormatError, "invalid info file header");
        }

        *format = cvtZToUInt(strZ(strNewZN(headerZ + INFO_HEADER_MAGIC_SIZE, INFO_HEADER_FORMAT_SIZE)));

        // Error on a format this version cannot read before anything is decrypted, since decrypting requires knowing what the
        // format expects and this version does not know what a newer format expects
        infoFormatValidate(*format);

        // The format is one this version knows, so the byte held back for later must be the one this version writes
        if (headerZ[INFO_HEADER_SIZE - 1] != INFO_HEADER_RESERVED)
            THROW(FormatError, "invalid info file header");
    }
    // Else the bytes must be the magic the cipher writes, since that is all a file with no header can begin with. Say so the way
    // the cipher would have, as this is what a file that was never encrypted looks like from here.
    else if (bufUsed(header) == INFO_HEADER_SIZE && memcmp(bufPtrConst(header), CIPHER_BLOCK_MAGIC, CIPHER_BLOCK_MAGIC_SIZE) != 0)
        THROW(CryptoError, "cipher header invalid");

    // Decrypt the content into a buffer
    Buffer *const result = bufNew(ioBufferSize());

    MEM_CONTEXT_TEMP_BEGIN()
    {
        IoWrite *const write = ioBufferWriteNew(result);
        ioFilterGroupAdd(
            ioWriteFilterGroup(write),
            cipherBlockNewP(cipherModeDecrypt, infoFormatCipherSpec(*format, cipherSpec), .raw = true));
        ioWriteOpen(write);

        Buffer *const chunk = bufNew(ioBufferSize());

        while (!ioReadEof(read))
        {
            bufUsedZero(chunk);
            ioRead(read, chunk);
            ioWrite(write, chunk);
        }

        ioWriteClose(write);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(IO_READ, ioBufferReadNew(result));
}

/**********************************************************************************************************************************/
#define INFO_SECTION_BACKREST                                       "backrest"
#define INFO_KEY_CHECKSUM                                           "backrest-checksum"
#define INFO_SECTION_CIPHER                                         "cipher"
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
            unsigned int formatHeader = 0;                                      // Format the header gave, 0 when there is none
            IoRead *contentRead = read;                                         // Read the content comes from

            INFO_CHECKSUM_BEGIN(checksumActualFilter);

            TRY_BEGIN()
            {
                if (cipherSpecType(cipherSpec) != cipherTypeNone)
                {
                    // A file that carries a header must be read past it before the content can be parsed
                    if (param.header)
                        contentRead = infoContentRead(read, cipherSpec, &formatHeader);
                    // Else the content is decrypted as it is parsed
                    else
                        cipherBlockFilterGroupAdd(ioReadFilterGroup(read), cipherModeDecrypt, cipherSpec);
                }

                Ini *const ini = iniNewP(contentRead, .strict = true);

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
                                const uint64_t format = varUInt64(jsonToVar(value->value));
                                infoFormatValidate(format);

                                // The header is written from the same format as the content, so a file where they disagree has
                                // been damaged or put together from parts of two files
                                if (formatHeader != 0 && format != formatHeader)
                                {
                                    THROW_FMT(
                                        FormatError, "repository format %" PRIu64 " does not match header format %u", format,
                                        formatHeader);
                                }

                                this->pub.format = (unsigned int)format;
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
                            // No validation needed for cipher-pass, just store it
                            if (strEqZ(value->key, INFO_KEY_CIPHER_PASS))
                            {
                                MEM_CONTEXT_OBJ_BEGIN(this)
                                {
                                    // The dependent files are encrypted with the same cipher type as this one and derive with the
                                    // digest that goes with the format this file was written at. The format is read before this
                                    // since the sections come out in order and backrest sorts before cipher.
                                    this->pub.cipherSpec = cipherSpecNewP(
                                        cipherSpecType(cipherSpec), BUFSTR(varStr(jsonToVar(value->value))),
                                        .digest = infoFormatDigest(this->pub.format));
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

        // A file with no cipher section has no encrypted dependent files
        if (this->pub.cipherSpec == NULL)
            infoCipherSpecSet(this, NULL);
    }
    OBJ_NEW_END();

    FUNCTION_LOG_RETURN(INFO, this);
}

/**********************************************************************************************************************************/
FN_EXTERN IoWrite *
infoWriteNew(Buffer *const buffer, const unsigned int format, const CipherSpec *const cipherSpec)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(BUFFER, buffer);
        FUNCTION_LOG_PARAM(UINT, format);
        FUNCTION_LOG_PARAM(CIPHER_SPEC, cipherSpec);
    FUNCTION_LOG_END();

    FUNCTION_AUDIT_HELPER();

    ASSERT(buffer != NULL);
    ASSERT(format >= REPOSITORY_FORMAT_MIN && format <= REPOSITORY_FORMAT_MAX);
    ASSERT(cipherSpec != NULL);

    // Write the header before the encryption filter is added so it stays plaintext. Format 5 gets none since it is the format a
    // reader assumes when there is nothing to say otherwise.
    const bool header = cipherSpecType(cipherSpec) != cipherTypeNone && format >= REPOSITORY_FORMAT_6;

    if (header)
        bufCat(buffer, BUFSTR(strNewFmt(INFO_HEADER_MAGIC "%0*u%c", INFO_HEADER_FORMAT_SIZE, format, INFO_HEADER_RESERVED)));

    IoWrite *const result = ioBufferWriteNew(buffer);

    // Encrypt raw when there is a header since the header takes the place of the magic the cipher would write
    if (cipherSpecType(cipherSpec) != cipherTypeNone)
    {
        ioFilterGroupAdd(
            ioWriteFilterGroup(result),
            cipherBlockNewP(cipherModeEncrypt, infoFormatCipherSpec(format, cipherSpec), .raw = header));
    }

    FUNCTION_LOG_RETURN(IO_WRITE, result);
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
