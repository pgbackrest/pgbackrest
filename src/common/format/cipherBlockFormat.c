/***********************************************************************************************************************************
Cipher Block Format
***********************************************************************************************************************************/
#include <build.h>

#include <ctype.h>
#include <string.h>

#include "common/crypto/cipherBlock.h"
#include "common/debug.h"
#include "common/format/cipherBlockFormat.h"
#include "common/format/format.h"
#include "common/io/filter/filter.h"
#include "common/log.h"
#include "common/type/convert.h"
#include "common/type/object.h"
#include "common/type/pack.h"

/***********************************************************************************************************************************
Format header

A file written at a format that has one begins with fixed-size plaintext naming the format it was written with. The header exists
because the digest the pass derives with follows the format, and the format is recorded inside the file that the pass encrypts. A
reader that could not see the format in advance would have to decrypt to learn what it should have decrypted with.

The header takes the place of the salted magic, which is why the content behind it is encrypted raw. Both are eight bytes followed
by the salt, so the eight bytes are consumed either way and what follows begins with the salt no matter which was there. It also
means a file of either kind is opened with the openssl command-line tool the same way: replace the first eight bytes with the magic
that tool expects.

A file that begins with the magic rather than the header was written at format 5, the only format there was before the header, so
that is not an error when a header was expected.

For the four bytes after the header magic, the first three are the format and the last is reserved for future use, e.g. naming
which key the file was encrypted with once a repository can hold more than one. The format comes first so that it is always at the
same place, which is what lets a version work out whether it can read the file at all. Once the format is identified as compatible
with this version, the spare byte is examined, and it must only be the underscore this version writes.
***********************************************************************************************************************************/
#define CIPHER_BLOCK_FORMAT_MAGIC                                   "PGBR"
#define CIPHER_BLOCK_FORMAT_MAGIC_SIZE                              (sizeof(CIPHER_BLOCK_FORMAT_MAGIC) - 1)
#define CIPHER_BLOCK_FORMAT_RESERVED                                '_'
#define CIPHER_BLOCK_FORMAT_SIZE                                    3

// Total length of the header, which is the magic, the format, and the reserved byte
#define CIPHER_BLOCK_FORMAT_HEADER_SIZE                                                                                            \
    (CIPHER_BLOCK_FORMAT_MAGIC_SIZE + CIPHER_BLOCK_FORMAT_SIZE + 1)

// The header is written in place of the magic, so the two must add up to exactly the same size or the salt would no longer begin at
// the same place
static_assert(
    CIPHER_BLOCK_FORMAT_HEADER_SIZE == CIPHER_BLOCK_MAGIC_SIZE, "cipher header must be the size of the magic it replaces");

// A format too large for the digits it is written in would be truncated to a different format
static_assert(REPOSITORY_FORMAT_MAX < 1000, "repository format must fit in the header digits");
static_assert(CIPHER_BLOCK_FORMAT_SIZE == 3, "format digits must match the digits the header is written with");

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct CipherBlockFormat
{
    const CipherSpec *cipherSpec;                                   // Cipher spec the content is decrypted with
    unsigned int formatExpected;                                    // Format the caller expects, zero when any will do
    unsigned int format;                                            // Format the header gave

    uint8_t header[CIPHER_BLOCK_FORMAT_HEADER_SIZE];                // Header bytes held until there are enough to read
    size_t headerSize;                                              // Header bytes held so far
    IoFilter *cipherBlock;                                          // Block cipher the content behind the header is decrypted by
    size_t sourceOffset;                                            // Bytes of the current source taken for the header
} CipherBlockFormat;

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
static void
cipherBlockFormatToLog(const CipherBlockFormat *const this, StringStatic *const debugLog)
{
    strStcFmt(debugLog, "{format: %u}", this->format);
}

#define FUNCTION_LOG_CIPHER_BLOCK_FORMAT_TYPE                                                                                      \
    CipherBlockFormat *
#define FUNCTION_LOG_CIPHER_BLOCK_FORMAT_FORMAT(value, buffer, bufferSize)                                                         \
    FUNCTION_LOG_OBJECT_FORMAT(value, cipherBlockFormatToLog, buffer, bufferSize)

/***********************************************************************************************************************************
Read the header and return the format it gives. A file that begins with the magic instead was written at format 5, the only format
there was before the header, so that start is not an error here.
***********************************************************************************************************************************/
static unsigned int
cipherBlockFormatHeaderRead(const uint8_t *const header)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM_P(BYTEDATA, header);
    FUNCTION_TEST_END();

    ASSERT(header != NULL);

    const char *const headerZ = (const char *)header;
    unsigned int result = REPOSITORY_FORMAT_5;

    if (memcmp(headerZ, CIPHER_BLOCK_FORMAT_MAGIC, CIPHER_BLOCK_FORMAT_MAGIC_SIZE) == 0)
    {
        for (unsigned int digitIdx = 0; digitIdx < CIPHER_BLOCK_FORMAT_SIZE; digitIdx++)
        {
            if (!isdigit((unsigned char)headerZ[CIPHER_BLOCK_FORMAT_MAGIC_SIZE + digitIdx]))
                THROW(FormatError, "invalid cipher header");
        }

        result = cvtZSubNToUInt(headerZ, CIPHER_BLOCK_FORMAT_MAGIC_SIZE, CIPHER_BLOCK_FORMAT_SIZE);

        // Error on a format this version cannot read before anything is decrypted
        repoFormatValidate(result);

        // The format is one this version can read, so the reserved byte must be the value this version writes
        if (headerZ[CIPHER_BLOCK_FORMAT_HEADER_SIZE - 1] != CIPHER_BLOCK_FORMAT_RESERVED)
            THROW(FormatError, "invalid cipher header");
    }
    // Else the bytes must be the magic, since that is all a file with no header can begin with
    else if (memcmp(headerZ, CIPHER_BLOCK_MAGIC, CIPHER_BLOCK_MAGIC_SIZE) != 0)
        THROW(CryptoError, "cipher header invalid");

    FUNCTION_TEST_RETURN(UINT, result);
}

/***********************************************************************************************************************************
Build the block cipher the content behind the header is decrypted by, which is only possible once the header has been read since the
header is what gives the format and the format is what gives the digest
***********************************************************************************************************************************/
static void
cipherBlockFormatCipherNew(CipherBlockFormat *const this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(CIPHER_BLOCK_FORMAT, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(this->cipherBlock == NULL);

    this->format = cipherBlockFormatHeaderRead(this->header);

    // Error when the format the caller expected is not the one the file was written with
    if (this->formatExpected != 0 && this->formatExpected != this->format)
        THROW_FMT(FormatError, "expected repository format %u but found %u", this->formatExpected, this->format);

    // The eight bytes at the front have been read and are not passed on, whichever of the two they were, so what reaches the block
    // cipher begins with the salt either way and it is told to expect no header of its own
    MEM_CONTEXT_OBJ_BEGIN(this)
    {
        this->cipherBlock = cipherBlockNewP(
            cipherModeDecrypt,
            cipherSpecNewP(
                cipherSpecType(this->cipherSpec), cipherSpecPass(this->cipherSpec), .digest = repoFormatDigest(this->format)),
            .header = cipherBlockHeaderNone);
    }
    MEM_CONTEXT_OBJ_END();

    FUNCTION_TEST_RETURN_VOID();
}

/***********************************************************************************************************************************
Read the header and pass everything behind it through the block cipher the header selected
***********************************************************************************************************************************/
static void
cipherBlockFormatProcess(THIS_VOID, const Buffer *const source, Buffer *const destination)
{
    THIS(CipherBlockFormat);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(CIPHER_BLOCK_FORMAT, this);
        FUNCTION_LOG_PARAM(BUFFER, source);
        FUNCTION_LOG_PARAM(BUFFER, destination);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(destination != NULL);

    if (source != NULL)
    {
        // Hold back the header until there is enough of it to read
        if (this->cipherBlock == NULL)
        {
            this->sourceOffset = CIPHER_BLOCK_FORMAT_HEADER_SIZE - this->headerSize;

            if (this->sourceOffset > bufUsed(source))
                this->sourceOffset = bufUsed(source);

            memcpy(this->header + this->headerSize, bufPtrConst(source), this->sourceOffset);
            this->headerSize += this->sourceOffset;

            // Nothing can be decrypted until the header is complete
            if (this->headerSize < CIPHER_BLOCK_FORMAT_HEADER_SIZE)
                FUNCTION_LOG_RETURN_VOID();

            cipherBlockFormatCipherNew(this);
        }

        // Decrypt whatever of the source the header did not take
        if (this->sourceOffset < bufUsed(source))
        {
            ioFilterProcessInOut(
                this->cipherBlock, BUF(bufPtrConst(source) + this->sourceOffset, bufUsed(source) - this->sourceOffset),
                destination);
        }

        // The header is stripped from the source only once
        if (!ioFilterInputSame(this->cipherBlock))
            this->sourceOffset = 0;
    }
    // Else all the input has been seen, so the block cipher is flushed
    else
    {
        // A file too short to hold a header cannot have one
        if (this->cipherBlock == NULL)
            THROW(CryptoError, "cipher header missing");

        ioFilterProcessInOut(this->cipherBlock, NULL, destination);
    }

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Is the filter done?
***********************************************************************************************************************************/
static bool
cipherBlockFormatDone(const THIS_VOID)
{
    THIS(const CipherBlockFormat);

    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(CIPHER_BLOCK_FORMAT, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(BOOL, this->cipherBlock != NULL && ioFilterDone(this->cipherBlock));
}

/***********************************************************************************************************************************
Should the same input be provided again?
***********************************************************************************************************************************/
static bool
cipherBlockFormatInputSame(const THIS_VOID)
{
    THIS(const CipherBlockFormat);

    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(CIPHER_BLOCK_FORMAT, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(BOOL, this->cipherBlock != NULL && ioFilterInputSame(this->cipherBlock));
}

/***********************************************************************************************************************************
Report the format the header gave
***********************************************************************************************************************************/
static Pack *
cipherBlockFormatResultPack(THIS_VOID)
{
    THIS(CipherBlockFormat);

    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(CIPHER_BLOCK_FORMAT, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    Pack *result;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        PackWrite *const packWrite = pckWriteNewP();

        pckWriteU32P(packWrite, this->format);
        pckWriteEndP(packWrite);

        result = pckMove(pckWriteResult(packWrite), memContextPrior());
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_TEST_RETURN(PACK, result);
}

/**********************************************************************************************************************************/
FN_EXTERN IoFilter *
cipherBlockFormatNew(const CipherSpec *const cipherSpec, const CipherBlockFormatNewParam param)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(CIPHER_SPEC, cipherSpec);
        FUNCTION_LOG_PARAM(UINT, param.format);
    FUNCTION_LOG_END();

    FUNCTION_AUDIT_HELPER();

    ASSERT(cipherSpec != NULL);
    ASSERT(cipherSpecType(cipherSpec) != cipherTypeNone);

    OBJ_NEW_BEGIN(CipherBlockFormat, .childQty = MEM_CONTEXT_QTY_MAX)
    {
        *this = (CipherBlockFormat)
        {
            .cipherSpec = cipherSpecDup(cipherSpec),
            .formatExpected = param.format,
        };
    }
    OBJ_NEW_END();

    // Create param list
    Pack *paramList;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        PackWrite *const packWrite = pckWriteNewP();

        cipherSpecPack(packWrite, cipherSpec);
        pckWriteU32P(packWrite, param.format);
        pckWriteEndP(packWrite);

        paramList = pckMove(pckWriteResult(packWrite), memContextPrior());
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(
        IO_FILTER,
        ioFilterNewP(
            CIPHER_BLOCK_FORMAT_FILTER_TYPE, this, paramList, .done = cipherBlockFormatDone,
            .inOut = cipherBlockFormatProcess, .inputSame = cipherBlockFormatInputSame,
            .result = cipherBlockFormatResultPack));
}

FN_EXTERN IoFilter *
cipherBlockFormatNewPack(const Pack *const paramList)
{
    IoFilter *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        PackRead *const paramListPack = pckReadNew(paramList);
        const CipherSpec *const cipherSpec = cipherSpecNewPack(paramListPack);
        const unsigned int format = pckReadU32P(paramListPack);

        result = ioFilterMove(cipherBlockFormatNewP(cipherSpec, .format = format), memContextPrior());
    }
    MEM_CONTEXT_TEMP_END();

    return result;
}

/**********************************************************************************************************************************/
FN_EXTERN unsigned int
cipherBlockFormatResult(PackRead *const packRead)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PACK_READ, packRead);
    FUNCTION_TEST_END();

    ASSERT(packRead != NULL);

    FUNCTION_TEST_RETURN(UINT, pckReadU32P(packRead));
}

/**********************************************************************************************************************************/
FN_EXTERN void
cipherBlockFormatFilterGroupWriteAdd(
    Buffer *const buffer, IoFilterGroup *const filterGroup, const CipherSpec *const cipherSpec, const unsigned int format)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(BUFFER, buffer);
        FUNCTION_LOG_PARAM(IO_FILTER_GROUP, filterGroup);
        FUNCTION_LOG_PARAM(CIPHER_SPEC, cipherSpec);
        FUNCTION_LOG_PARAM(UINT, format);
    FUNCTION_LOG_END();

    FUNCTION_AUDIT_HELPER();

    ASSERT(buffer != NULL);
    ASSERT(filterGroup != NULL);
    ASSERT(cipherSpec != NULL);
    ASSERT(format >= REPOSITORY_FORMAT_MIN && format <= REPOSITORY_FORMAT_MAX);

    if (cipherSpecType(cipherSpec) != cipherTypeNone)
    {
        // A format from 6 writes the header in place of the magic, so the content behind it is encrypted raw
        const bool header = format >= REPOSITORY_FORMAT_6;

        if (header)
        {
            char headerZ[CIPHER_BLOCK_FORMAT_HEADER_SIZE + 1];

            // Write the format zero-padded so it is always the same size, taking the digits it is written in from the value so
            // that a format too large to fit could not run past them. The terminator lands on the byte after the header, which is
            // not written to the buffer.
            snprintf(
                headerZ, sizeof(headerZ), CIPHER_BLOCK_FORMAT_MAGIC "%03u%c", format % 1000, CIPHER_BLOCK_FORMAT_RESERVED);

            bufCatC(buffer, (const uint8_t *)headerZ, 0, CIPHER_BLOCK_FORMAT_HEADER_SIZE);
        }

        ioFilterGroupAdd(
            filterGroup,
            cipherBlockNewP(
                cipherModeEncrypt,
                cipherSpecNewP(
                    cipherSpecType(cipherSpec), cipherSpecPass(cipherSpec), .digest = repoFormatDigest(format)),
                .header = header ? cipherBlockHeaderNone : cipherBlockHeaderMagic));
    }

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
FN_EXTERN IoFilterGroup *
cipherBlockFormatFilterGroupReadAdd(IoFilterGroup *const filterGroup, const CipherSpec *const cipherSpec)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(IO_FILTER_GROUP, filterGroup);
        FUNCTION_LOG_PARAM(CIPHER_SPEC, cipherSpec);
    FUNCTION_LOG_END();

    FUNCTION_AUDIT_HELPER();

    ASSERT(filterGroup != NULL);
    ASSERT(cipherSpec != NULL);

    if (cipherSpecType(cipherSpec) != cipherTypeNone)
        ioFilterGroupAdd(filterGroup, cipherBlockFormatNewP(cipherSpec));

    FUNCTION_LOG_RETURN(IO_FILTER_GROUP, filterGroup);
}
