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

The first three bytes of the header magic are the format and the last is a marker indicating whether a key id follows. The format
comes first so it is always in the same place, which is how a version decides whether it can read the file at all. The marker is
only checked once the format turns out to be readable, and it must be valid for this version.

If a key id is indicated by the marker, there is first a byte to indicate the length of the key id and then the key id as a string.
***********************************************************************************************************************************/
#define CIPHER_BLOCK_FORMAT_MAGIC                                   "PGBR"
#define CIPHER_BLOCK_FORMAT_MAGIC_SIZE                              (sizeof(CIPHER_BLOCK_FORMAT_MAGIC) - 1)
#define CIPHER_BLOCK_FORMAT_MARKER_NONE                             '_'
#define CIPHER_BLOCK_FORMAT_MARKER_KEY                              'K'
#define CIPHER_BLOCK_FORMAT_SIZE                                    3

// They key id length is encoded as a single byte with a max of 255 characters
#define CIPHER_BLOCK_FORMAT_KEY_SIZE_MAX                            255

// Total header length: magic, format, and marker
#define CIPHER_BLOCK_FORMAT_HEADER_SIZE                             (CIPHER_BLOCK_FORMAT_MAGIC_SIZE + CIPHER_BLOCK_FORMAT_SIZE + 1)

// The header is written in place of the magic, so the two must add up to exactly the same size or the salt would no longer begin at
// the same place
static_assert(
    CIPHER_BLOCK_FORMAT_HEADER_SIZE == CIPHER_BLOCK_MAGIC_SIZE, "cipher header must be the size of the magic it replaces");

// A format too large for the digits it is written in would be truncated to a different format
static_assert(REPOSITORY_FORMAT_MAX < 1000, "repository format must fit in the header digits");
static_assert(CIPHER_BLOCK_FORMAT_SIZE == 3, "format digits must match the digits the header is written with");

// Max possible size of the header, the fixed part plus a length byte and the longest key id it can contain
#define CIPHER_BLOCK_FORMAT_HEADER_SIZE_MAX                                                                                        \
    (CIPHER_BLOCK_FORMAT_HEADER_SIZE + 1 + CIPHER_BLOCK_FORMAT_KEY_SIZE_MAX)

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct CipherBlockFormat
{
    const CipherSpecMap *cipherSpecMap;                             // Keys to choose from
    unsigned int formatExpected;                                    // Format the caller expects, zero when any will do
    unsigned int format;                                            // Format the header gave

    uint8_t header[CIPHER_BLOCK_FORMAT_HEADER_SIZE_MAX];            // Header bytes held until there are enough to read
    size_t headerSize;                                              // Header bytes held so far
    size_t headerSizeExpected;                                      // Total header size, which grows as the header is read
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

        // Check that the marker is valid for this version
        if (headerZ[CIPHER_BLOCK_FORMAT_HEADER_SIZE - 1] != CIPHER_BLOCK_FORMAT_MARKER_NONE &&
            headerZ[CIPHER_BLOCK_FORMAT_HEADER_SIZE - 1] != CIPHER_BLOCK_FORMAT_MARKER_KEY)
        {
            THROW(FormatError, "invalid cipher header");
        }
    }
    // Else the bytes must be the magic, since that is all a file with no header can begin with
    else if (memcmp(headerZ, CIPHER_BLOCK_MAGIC, CIPHER_BLOCK_MAGIC_SIZE) != 0)
        THROW(CryptoError, "cipher header invalid");

    FUNCTION_TEST_RETURN(UINT, result);
}

/***********************************************************************************************************************************
Build the block cipher that decrypts the content, which is only possible once the header has been read since the header defines the
format and the key id
***********************************************************************************************************************************/
static void
cipherBlockFormatCipherNew(CipherBlockFormat *const this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(CIPHER_BLOCK_FORMAT, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(this->cipherBlock == NULL);

    // Error when the format the caller expected is not the one the file was written with
    if (this->formatExpected != 0 && this->formatExpected != this->format)
        THROW_FMT(FormatError, "expected repository format %u but found %u", this->formatExpected, this->format);

    // Get the key specified in the header, or the default key when it contains none
    const CipherSpec *cipherSpec;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        const String *keyId = CIPHER_SPEC_MAP_ID_DEFAULT_STR;

        if (this->headerSize > CIPHER_BLOCK_FORMAT_HEADER_SIZE)
        {
            keyId = strNewZN(
                (const char *)this->header + CIPHER_BLOCK_FORMAT_HEADER_SIZE + 1,
                this->headerSize - CIPHER_BLOCK_FORMAT_HEADER_SIZE - 1);
        }

        cipherSpec = cipherSpecMapGet(this->cipherSpecMap, keyId);
    }
    MEM_CONTEXT_TEMP_END();

    // Decrypt raw since the format header has already been parsed
    MEM_CONTEXT_OBJ_BEGIN(this)
    {
        this->cipherBlock = cipherBlockNewP(
            cipherModeDecrypt, cipherSpecDupP(cipherSpec, .digestDefault = repoFormatDigest(this->format)),
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
        // Read until the header is complete. The total size is not known until the marker and key id length have been read.
        if (this->cipherBlock == NULL)
        {
            this->sourceOffset = 0;

            do
            {
                size_t copySize = this->headerSizeExpected - this->headerSize;

                if (copySize > bufUsed(source) - this->sourceOffset)
                    copySize = bufUsed(source) - this->sourceOffset;

                memcpy(this->header + this->headerSize, bufPtrConst(source) + this->sourceOffset, copySize);
                this->headerSize += copySize;
                this->sourceOffset += copySize;

                // Nothing can be decrypted until the header is complete
                if (this->headerSize < this->headerSizeExpected)
                    FUNCTION_LOG_RETURN_VOID();

                // Parse the fixed part first to reject an invalid header before using the marker
                if (this->headerSizeExpected == CIPHER_BLOCK_FORMAT_HEADER_SIZE)
                {
                    this->format = cipherBlockFormatHeaderRead(this->header);

                    if (this->header[CIPHER_BLOCK_FORMAT_HEADER_SIZE - 1] == CIPHER_BLOCK_FORMAT_MARKER_KEY)
                        this->headerSizeExpected++;
                }
                // Else the length is complete, so wait for the key id
                else if (this->headerSizeExpected == CIPHER_BLOCK_FORMAT_HEADER_SIZE + 1)
                {
                    const size_t keySize = this->header[CIPHER_BLOCK_FORMAT_HEADER_SIZE];

                    // Marker indicates a key id but the length is zero
                    if (keySize == 0)
                        THROW(FormatError, "invalid cipher header");

                    this->headerSizeExpected += keySize;
                }
            }
            while (this->headerSize < this->headerSizeExpected);

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
cipherBlockFormatNew(const CipherSpecMap *const cipherSpecMap, const CipherBlockFormatNewParam param)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(CIPHER_SPEC_MAP, cipherSpecMap);
        FUNCTION_LOG_PARAM(UINT, param.format);
    FUNCTION_LOG_END();

    FUNCTION_AUDIT_HELPER();

    ASSERT(cipherSpecMap != NULL);
    ASSERT(cipherSpecMapSize(cipherSpecMap) != 0);

    OBJ_NEW_BEGIN(CipherBlockFormat, .childQty = MEM_CONTEXT_QTY_MAX)
    {
        *this = (CipherBlockFormat)
        {
            .cipherSpecMap = cipherSpecMapDup(cipherSpecMap),
            .formatExpected = param.format,
            .headerSizeExpected = CIPHER_BLOCK_FORMAT_HEADER_SIZE,
        };
    }
    OBJ_NEW_END();

    // Create param list
    Pack *paramList;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        PackWrite *const packWrite = pckWriteNewP();

        cipherSpecMapPack(packWrite, cipherSpecMap);
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
        const CipherSpecMap *const cipherSpecMap = cipherSpecMapNewPack(paramListPack);
        const unsigned int format = pckReadU32P(paramListPack);

        result = ioFilterMove(cipherBlockFormatNewP(cipherSpecMap, .format = format), memContextPrior());
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

/***********************************************************************************************************************************
Filter to write the header before the content. The header must be plaintext so this filter runs after the block cipher.
***********************************************************************************************************************************/
typedef struct CipherBlockFormatHeader
{
    const Buffer *header;                                           // Header to write before any content
    size_t headerOffset;                                            // Header bytes written so far
    size_t sourceOffset;                                            // Content bytes written from the current source
    bool inputSame;                                                 // Is the same input required on the next process call?
} CipherBlockFormatHeader;

static void
cipherBlockFormatHeaderProcess(THIS_VOID, const Buffer *const source, Buffer *const destination)
{
    THIS(CipherBlockFormatHeader);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM_P(VOID, this);
        FUNCTION_LOG_PARAM(BUFFER, source);
        FUNCTION_LOG_PARAM(BUFFER, destination);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(source != NULL);
    ASSERT(destination != NULL);

    // Write the header before any content
    if (this->headerOffset < bufUsed(this->header))
    {
        size_t copySize = bufUsed(this->header) - this->headerOffset;

        if (copySize > bufRemains(destination))
            copySize = bufRemains(destination);

        bufCatC(destination, bufPtrConst(this->header), this->headerOffset, copySize);
        this->headerOffset += copySize;
    }

    // Write the content once the header is complete
    if (this->headerOffset == bufUsed(this->header))
    {
        size_t copySize = bufUsed(source) - this->sourceOffset;

        if (copySize > bufRemains(destination))
            copySize = bufRemains(destination);

        bufCatC(destination, bufPtrConst(source), this->sourceOffset, copySize);
        this->sourceOffset += copySize;
    }

    this->inputSame = this->headerOffset < bufUsed(this->header) || this->sourceOffset < bufUsed(source);

    // Reset source offset when source has been completely processed
    if (!this->inputSame)
        this->sourceOffset = 0;

    FUNCTION_LOG_RETURN_VOID();
}

static bool
cipherBlockFormatHeaderInputSame(const THIS_VOID)
{
    THIS(const CipherBlockFormatHeader);

    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM_P(VOID, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(BOOL, this->inputSame);
}

static IoFilter *
cipherBlockFormatHeaderNew(const Buffer *const header)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(BUFFER, header);
    FUNCTION_TEST_END();

    FUNCTION_AUDIT_HELPER();

    ASSERT(header != NULL);
    ASSERT(!bufEmpty(header));

    OBJ_NEW_BEGIN(CipherBlockFormatHeader, .childQty = MEM_CONTEXT_QTY_MAX)
    {
        *this = (CipherBlockFormatHeader){.header = bufDup(header)};
    }
    OBJ_NEW_END();

    // Create param list
    Pack *paramList;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        PackWrite *const packWrite = pckWriteNewP();

        pckWriteBinP(packWrite, header);
        pckWriteEndP(packWrite);

        paramList = pckMove(pckWriteResult(packWrite), memContextPrior());
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_TEST_RETURN(
        IO_FILTER,
        ioFilterNewP(
            CIPHER_BLOCK_FORMAT_HEADER_FILTER_TYPE, this, paramList, .inOut = cipherBlockFormatHeaderProcess,
            .inputSame = cipherBlockFormatHeaderInputSame));
}

FN_EXTERN IoFilter *
cipherBlockFormatHeaderNewPack(const Pack *const paramList)
{
    IoFilter *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        result = ioFilterMove(cipherBlockFormatHeaderNew(pckReadBinP(pckReadNew(paramList))), memContextPrior());
    }
    MEM_CONTEXT_TEMP_END();

    return result;
}

/**********************************************************************************************************************************/
FN_EXTERN void
cipherBlockFormatFilterGroupWriteAdd(
    IoFilterGroup *const filterGroup, const CipherSpec *const cipherSpec, const unsigned int format,
    const CipherBlockFormatFilterGroupWriteAddParam param)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(IO_FILTER_GROUP, filterGroup);
        FUNCTION_LOG_PARAM(CIPHER_SPEC, cipherSpec);
        FUNCTION_LOG_PARAM(UINT, format);
        FUNCTION_LOG_PARAM(STRING, param.keyId);
    FUNCTION_LOG_END();

    FUNCTION_AUDIT_HELPER();

    ASSERT(filterGroup != NULL);
    ASSERT(cipherSpec != NULL);
    ASSERT(format >= REPOSITORY_FORMAT_MIN && format <= REPOSITORY_FORMAT_MAX);
    ASSERT(param.keyId == NULL || !strEmpty(param.keyId));
    ASSERT(param.keyId == NULL || strSize(param.keyId) <= CIPHER_BLOCK_FORMAT_KEY_SIZE_MAX);

    // Key id is only valid for format >= 6
    ASSERT(param.keyId == NULL || format >= REPOSITORY_FORMAT_6);

    if (cipherSpecType(cipherSpec) != cipherTypeNone)
    {
        // Format >= 6 writes the header in place of the magic and the content behind it is encrypted raw
        const bool header = format >= REPOSITORY_FORMAT_6;

        ioFilterGroupAdd(
            filterGroup,
            cipherBlockNewP(
                cipherModeEncrypt, cipherSpecDupP(cipherSpec, .digestDefault = repoFormatDigest(format)),
                .header = header ? cipherBlockHeaderNone : cipherBlockHeaderMagic));

        // The header is plaintext, so it is added after the block cipher and lands in front of what the cipher produces
        if (header)
        {
            MEM_CONTEXT_TEMP_BEGIN()
            {
                char headerZ[CIPHER_BLOCK_FORMAT_HEADER_SIZE + 1];

                // Zero-pad the format and add the marker, depending on whether or not there is a key
                snprintf(
                    headerZ, sizeof(headerZ), CIPHER_BLOCK_FORMAT_MAGIC "%03u%c", format % 1000,
                    param.keyId == NULL ? CIPHER_BLOCK_FORMAT_MARKER_NONE : CIPHER_BLOCK_FORMAT_MARKER_KEY);

                Buffer *const headerBuffer = bufNew(CIPHER_BLOCK_FORMAT_HEADER_SIZE_MAX);
                bufCatC(headerBuffer, (const uint8_t *)headerZ, 0, CIPHER_BLOCK_FORMAT_HEADER_SIZE);

                // Write the key size and id when present
                if (param.keyId != NULL)
                {
                    const uint8_t keySize = (uint8_t)strSize(param.keyId);

                    bufCatC(headerBuffer, &keySize, 0, sizeof(keySize));
                    bufCatC(headerBuffer, (const uint8_t *)strZ(param.keyId), 0, strSize(param.keyId));
                }

                ioFilterGroupAdd(filterGroup, cipherBlockFormatHeaderNew(headerBuffer));
            }
            MEM_CONTEXT_TEMP_END();
        }
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
    {
        MEM_CONTEXT_TEMP_BEGIN()
        {
            CipherSpecMap *const cipherSpecMap = cipherSpecMapNew();
            cipherSpecMapAdd(cipherSpecMap, CIPHER_SPEC_MAP_ID_DEFAULT_STR, cipherSpec);

            ioFilterGroupAdd(filterGroup, cipherBlockFormatNewP(cipherSpecMap));
        }
        MEM_CONTEXT_TEMP_END();
    }

    FUNCTION_LOG_RETURN(IO_FILTER_GROUP, filterGroup);
}

/**********************************************************************************************************************************/
FN_EXTERN IoFilterGroup *
cipherBlockFormatFilterGroupReadAddMap(IoFilterGroup *const filterGroup, const CipherSpecMap *const cipherSpecMap)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(IO_FILTER_GROUP, filterGroup);
        FUNCTION_LOG_PARAM(CIPHER_SPEC_MAP, cipherSpecMap);
    FUNCTION_LOG_END();

    FUNCTION_AUDIT_HELPER();

    ASSERT(filterGroup != NULL);
    ASSERT(cipherSpecMap != NULL);

    // An unencrypted repository has no keys, so there is nothing to decrypt
    if (cipherSpecMapSize(cipherSpecMap) != 0)
        ioFilterGroupAdd(filterGroup, cipherBlockFormatNewP(cipherSpecMap));

    FUNCTION_LOG_RETURN(IO_FILTER_GROUP, filterGroup);
}
