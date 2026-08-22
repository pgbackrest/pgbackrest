/***********************************************************************************************************************************
Harness for Loading Test Configurations
***********************************************************************************************************************************/
#include <build.h>

#include <string.h>

#include "common/assert.h"
#include "common/crypto/cipherBlock.h"
#include "common/crypto/hash.h"
#include "common/format.h"
#include "common/io/bufferRead.h"
#include "common/io/bufferWrite.h"
#include "common/io/filter/filter.h"
#include "common/type/json.h"
#include "info/info.h"
#include "version.h"

#include "harness/config.h"
#include "harness/debug.h"
#include "harness/info.h"

/***********************************************************************************************************************************
Add header and checksum to an info file

This prevents churn in headers and checksums in the unit tests. We purposefully do not use the checksum macros from the info module
here as a cross-check of that code.
***********************************************************************************************************************************/
Buffer *
harnessInfoChecksumFormat(const unsigned int format, const String *info)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(UINT, format);
        FUNCTION_HARNESS_PARAM(STRING, info);
    FUNCTION_HARNESS_END();

    ASSERT(info != NULL);

    Buffer *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        const String *sectionLast = NULL;                           // The last section seen during load
        IoFilter *const checksum = cryptoHashNew(hashTypeSha1);     // Checksum calculated from the file

        // Create buffer with space for data, header, and checksum
        result = bufNew(strSize(info) + 256);

        bufCat(result, BUFSTRDEF("[backrest]\nbackrest-format="));
        bufCat(result, BUFSTR(jsonFromVar(VARUINT(format))));
        bufCat(result, BUFSTRDEF("\nbackrest-version="));
        bufCat(result, BUFSTR(jsonFromVar(VARSTRDEF(PROJECT_VERSION))));
        bufCat(result, BUFSTRDEF("\n\n"));
        bufCat(result, BUFSTR(info));

        // Generate checksum by loading ini file
        ioFilterProcessIn(checksum, BUFSTRDEF("{"));

        Ini *const ini = iniNewP(ioBufferReadNew(result), .strict = true);
        const IniValue *value = iniValueNext(ini);

        while (value != NULL)
        {
            if (sectionLast == NULL || !strEq(value->section, sectionLast))
            {
                if (sectionLast != NULL)
                    ioFilterProcessIn(checksum, BUFSTRDEF("},"));

                ioFilterProcessIn(checksum, BUFSTRDEF("\""));
                ioFilterProcessIn(checksum, BUFSTR(value->section));
                ioFilterProcessIn(checksum, BUFSTRDEF("\":{"));

                sectionLast = strDup(value->section);
            }
            else
                ioFilterProcessIn(checksum, BUFSTRDEF(","));

            ioFilterProcessIn(checksum, BUFSTR(jsonFromVar(VARSTR(value->key))));
            ioFilterProcessIn(checksum, BUFSTRDEF(":"));
            ioFilterProcessIn(checksum, BUFSTR(value->value));

            value = iniValueNext(ini);
        }

        ioFilterProcessIn(checksum, BUFSTRDEF("}}"));

        // Append checksum to buffer
        bufCat(result, BUFSTRDEF("\n[backrest]\nbackrest-checksum="));
        bufCat(result, BUFSTR(jsonFromVar(VARSTR(strNewEncode(encodingHex, pckReadBinP(pckReadNew(ioFilterResult(checksum))))))));
        bufCat(result, BUFSTRDEF("\n"));

        bufMove(result, memContextPrior());
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_HARNESS_RETURN(BUFFER, result);
}

Buffer *
harnessInfoChecksumZ(const char *info)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(STRINGZ, info);
    FUNCTION_HARNESS_END();

    ASSERT(info != NULL);

    FUNCTION_HARNESS_RETURN(BUFFER, harnessInfoChecksum(STR(info)));
}

Buffer *
harnessInfoChecksum(const String *const info)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(STRING, info);
    FUNCTION_HARNESS_END();

    ASSERT(info != NULL);

    FUNCTION_HARNESS_RETURN(BUFFER, harnessInfoChecksumFormat(REPOSITORY_FORMAT_DEFAULT, info));
}

/**********************************************************************************************************************************/
void
hrnInfoPut(const Storage *const storage, const char *const file, const char *const info, HrnInfoPutParam param)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(STORAGE, storage);
        FUNCTION_HARNESS_PARAM(STRINGZ, file);
        FUNCTION_HARNESS_PARAM(STRINGZ, info);
        FUNCTION_HARNESS_PARAM(UINT, param.format);
        FUNCTION_HARNESS_PARAM(BOOL, param.header);
        FUNCTION_HARNESS_PARAM(CIPHER_SPEC, param.cipherSpec);
        FUNCTION_HARNESS_PARAM(STRINGZ, param.comment);
    FUNCTION_HARNESS_END();

    ASSERT(info != NULL);

    // Default to the format a new repository is created at
    if (param.format == 0)
        param.format = REPOSITORY_FORMAT_DEFAULT;

    const Buffer *content = harnessInfoChecksumFormat(param.format, STR(info));

    // Encrypt the way the format stores the file. A file that contains a header gets it in place of the magic the cipher writes,
    // and from format 6 the pass derives with SHA-256 rather than SHA-1.
    if (param.cipherSpec != NULL && cipherSpecType(param.cipherSpec) != cipherTypeNone)
    {
        const bool header = param.header && param.format >= REPOSITORY_FORMAT_6;
        Buffer *const encrypted = bufNew(0);

        if (header)
            bufCat(encrypted, BUFSTR(strNewFmt("PGBR%03u_", param.format)));

        IoWrite *const write = ioBufferWriteNew(encrypted);
        ioFilterGroupAdd(
            ioWriteFilterGroup(write),
            cipherBlockNewP(
                cipherModeEncrypt,
                cipherSpecNewP(
                    cipherSpecType(param.cipherSpec), cipherSpecPass(param.cipherSpec),
                    .digest = param.format >= REPOSITORY_FORMAT_6 ? hashTypeSha256 : hashTypeSha1),
                .raw = header));

        ioWriteOpen(write);
        ioWrite(write, content);
        ioWriteClose(write);

        content = encrypted;
    }

    hrnStoragePut(storage, file, content, "put info", (HrnStoragePutParam){VAR_PARAM_INIT, .comment = param.comment});

    FUNCTION_HARNESS_RETURN_VOID();
}

/**********************************************************************************************************************************/
Buffer *
harnessInfoEncrypt(const Buffer *const content, const CipherSpec *const cipherSpec)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(BUFFER, content);
        FUNCTION_HARNESS_PARAM(CIPHER_SPEC, cipherSpec);
    FUNCTION_HARNESS_END();

    ASSERT(content != NULL);
    ASSERT(cipherSpec != NULL);

    Buffer *const result = bufNew(0);
    IoWrite *const write = ioBufferWriteNew(result);
    ioFilterGroupAdd(ioWriteFilterGroup(write), cipherBlockNewP(cipherModeEncrypt, cipherSpec));

    ioWriteOpen(write);
    ioWrite(write, content);
    ioWriteClose(write);

    FUNCTION_HARNESS_RETURN(BUFFER, result);
}

/***********************************************************************************************************************************
Test callback that logs the results to a string
***********************************************************************************************************************************/
void
harnessInfoLoadNewCallback(
    void *const callbackData, const String *const section, const String *const key, JsonRead *const json)
{
    if (callbackData != NULL)
        strCatFmt((String *)callbackData, "[%s] %s=%s\n", strZ(section), strZ(key), *(const char **)json);
}
