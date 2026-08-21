/***********************************************************************************************************************************
Block Cipher
***********************************************************************************************************************************/
#include <build.h>

#include <ctype.h>
#include <string.h>

#include <openssl/err.h>
#include <openssl/evp.h>

#include "common/crypto/cipherBlock.h"
#include "common/crypto/common.h"
#include "common/debug.h"
#include "common/format.h"
#include "common/io/filter/filter.h"
#include "common/log.h"
#include "common/type/convert.h"
#include "common/type/object.h"

/***********************************************************************************************************************************
Magic constant for salted encrypt, written before the salt unless the cipher is raw. Only salted encrypt is done here, but this
constant is required for compatibility with the openssl command-line tool.
***********************************************************************************************************************************/
#define CIPHER_BLOCK_MAGIC                                          "Salted__"
#define CIPHER_BLOCK_MAGIC_SIZE                                     (sizeof(CIPHER_BLOCK_MAGIC) - 1)

/***********************************************************************************************************************************
Format header

A file written with a header begins with fixed-size plaintext naming the repository format it was written with. The header exists
because the digest the pass derives with follows the format, and the format is recorded inside the file that the pass encrypts. A
reader that could not see the format in advance would have to decrypt to learn what it should have decrypted with.

The header takes the place of the salted magic, which is why a file that contains one is written raw. Both are eight bytes followed
by the salt, so the eight bytes are consumed either way and what follows begins with the salt no matter which was there. It also
means a file of either kind is opened with the openssl command-line tool the same way: replace the first eight bytes with the magic
that tool expects.

A file that begins with the magic rather than the header was written at format 5, the only format there was before the header, so
that is not an error when a header was expected.

For the four bytes after the header magic, the first three are the format and the last is reserved for future use, e.g. naming which
key the file was encrypted with once a repository can hold more than one. The format comes first so that it is always at the same
place, which is what lets a version work out whether it can read the file at all. Once the format is identified as compatible with
this version, the spare byte is examined, and it must only be the underscore this version writes.

The header is not part of the file content. This filter adds it on encrypt and consumes it on decrypt, so nothing on either side
sees anything but the content.
***********************************************************************************************************************************/
#define CIPHER_BLOCK_HEADER_MAGIC                                   "PGBR"
#define CIPHER_BLOCK_HEADER_MAGIC_SIZE                              (sizeof(CIPHER_BLOCK_HEADER_MAGIC) - 1)
#define CIPHER_BLOCK_HEADER_RESERVED                                '_'
#define CIPHER_BLOCK_HEADER_FORMAT_SIZE                             3

// Total length of cipher header
#define CIPHER_BLOCK_HEADER_SIZE                                    (CIPHER_BLOCK_MAGIC_SIZE + PKCS5_SALT_LEN)

/***********************************************************************************************************************************
Digest the pass derives the key with. The lookup is by name, so a digest must be one openssl knows.
***********************************************************************************************************************************/
static const EVP_MD *
cipherBlockDigest(const HashType type)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING_ID, type);
    FUNCTION_TEST_END();

    char typeZ[STRID_MAX + 1];
    strIdToZ(type, typeZ);

    const EVP_MD *const result = EVP_get_digestbyname(typeZ);

    if (result == NULL)
        THROW_FMT(AssertError, "unable to load digest '%s'", typeZ);

    FUNCTION_TEST_RETURN_TYPE_CONST_P(EVP_MD, result);
}

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct CipherBlock
{
    CipherMode mode;                                                // Mode encrypt/decrypt
    bool raw;                                                       // Omit header magic to save space
    bool headerFormat;                                              // Read/write the format header
    unsigned int format;                                            // Repository format, zero until a read header gives one
    bool saltDone;                                                  // Has the salt been read/generated?
    bool processDone;                                               // Has any data been processed?
    const Buffer *pass;                                             // Passphrase used to generate encryption key
    size_t headerSize;                                              // Size of header read during decrypt
    uint8_t header[CIPHER_BLOCK_HEADER_SIZE];                       // Buffer to hold partial header during decrypt
    const EVP_CIPHER *cipher;                                       // Cipher object
    const EVP_MD *digest;                                           // Message digest object
    EVP_CIPHER_CTX *cipherContext;                                  // Encrypt/decrypt context

    Buffer *buffer;                                                 // Internal buffer in case destination buffer isn't large enough
    bool inputSame;                                                 // Is the same input required on next process call?
    bool done;                                                      // Is processing done?
} CipherBlock;

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
static void
cipherBlockToLog(const CipherBlock *const this, StringStatic *const debugLog)
{
    strStcFmt(debugLog, "{inputSame: %s, done: %s}", cvtBoolToConstZ(this->inputSame), cvtBoolToConstZ(this->done));
}

#define FUNCTION_LOG_CIPHER_BLOCK_TYPE                                                                                             \
    CipherBlock *
#define FUNCTION_LOG_CIPHER_BLOCK_FORMAT(value, buffer, bufferSize)                                                                \
    FUNCTION_LOG_OBJECT_FORMAT(value, cipherBlockToLog, buffer, bufferSize)

/***********************************************************************************************************************************
Free cipher context
***********************************************************************************************************************************/
static void
cipherBlockFreeResource(THIS_VOID)
{
    THIS(CipherBlock);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(CIPHER_BLOCK, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    EVP_CIPHER_CTX_free(this->cipherContext);

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Determine how large the destination buffer should be
***********************************************************************************************************************************/
static size_t
cipherBlockProcessSize(const CipherBlock *const this, const size_t sourceSize)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(CIPHER_BLOCK, this);
        FUNCTION_LOG_PARAM(SIZE, sourceSize);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    // Destination size is source size plus one extra block
    size_t destinationSize = sourceSize + EVP_MAX_BLOCK_LENGTH;

    // On encrypt the header size must be included before the first block
    if (this->mode == cipherModeEncrypt && !this->saltDone)
        destinationSize += CIPHER_BLOCK_MAGIC_SIZE + PKCS5_SALT_LEN;

    FUNCTION_LOG_RETURN(SIZE, destinationSize);
}

/***********************************************************************************************************************************
Encrypt/decrypt data
***********************************************************************************************************************************/
static size_t
cipherBlockProcessBlock(CipherBlock *const this, const uint8_t *source, size_t sourceSize, uint8_t *destination)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(CIPHER_BLOCK, this);
        FUNCTION_LOG_PARAM_P(BYTEDATA, source);
        FUNCTION_LOG_PARAM(SIZE, sourceSize);
        FUNCTION_LOG_PARAM_P(BYTEDATA, destination);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(source != NULL || sourceSize == 0);
    ASSERT(destination != NULL);

    // Actual destination size
    size_t destinationSize = 0;

    // If the salt has not been generated/read yet
    if (!this->saltDone)
    {
        const uint8_t *salt = NULL;

        // On encrypt the salt is generated
        if (this->mode == cipherModeEncrypt)
        {
            // Add the header to the destination buffer in place of the magic. Both are the same size so what follows begins with
            // the salt either way.
            if (this->headerFormat)
            {
                memcpy(destination, CIPHER_BLOCK_HEADER_MAGIC, CIPHER_BLOCK_HEADER_MAGIC_SIZE);

                // Write the format zero-padded so it is always the same size. The terminator lands on the reserved byte, which is
                // written next.
                snprintf(
                    (char *)destination + CIPHER_BLOCK_HEADER_MAGIC_SIZE, CIPHER_BLOCK_HEADER_FORMAT_SIZE + 1, "%0*u",
                    CIPHER_BLOCK_HEADER_FORMAT_SIZE, this->format);

                destination[CIPHER_BLOCK_MAGIC_SIZE - 1] = CIPHER_BLOCK_HEADER_RESERVED;

                destination += CIPHER_BLOCK_MAGIC_SIZE;
                destinationSize += CIPHER_BLOCK_MAGIC_SIZE;
            }
            // Else add magic to the destination buffer so openssl knows the file is salted
            else if (!this->raw)
            {
                memcpy(destination, CIPHER_BLOCK_MAGIC, CIPHER_BLOCK_MAGIC_SIZE);
                destination += CIPHER_BLOCK_MAGIC_SIZE;
                destinationSize += CIPHER_BLOCK_MAGIC_SIZE;
            }

            // Add salt to the destination buffer
            cryptoRandomBytes(destination, PKCS5_SALT_LEN);
            salt = destination;
            destination += PKCS5_SALT_LEN;
            destinationSize += PKCS5_SALT_LEN;
        }
        // On decrypt the salt is read from the header
        else if (sourceSize > 0)
        {
            // Check if the entire header has been read
            const size_t headerExpected = this->raw ? PKCS5_SALT_LEN : CIPHER_BLOCK_HEADER_SIZE;

            if (this->headerSize + sourceSize >= headerExpected)
            {
                // Copy header (or remains of header) from source into the header buffer
                memcpy(this->header + this->headerSize, source, headerExpected - this->headerSize);
                salt = this->header + (this->raw ? 0 : CIPHER_BLOCK_MAGIC_SIZE);

                // Advance source and source size by the number of bytes read
                source += headerExpected - this->headerSize;
                sourceSize -= headerExpected - this->headerSize;

                // Read the format from the header. A file that begins with the magic instead was written at format 5, the only
                // format there was before the header, so that start is not an error here.
                if (this->headerFormat)
                {
                    const char *const headerZ = (const char *)this->header;
                    unsigned int format = REPOSITORY_FORMAT_5;

                    if (memcmp(headerZ, CIPHER_BLOCK_HEADER_MAGIC, CIPHER_BLOCK_HEADER_MAGIC_SIZE) == 0)
                    {
                        for (unsigned int digitIdx = 0; digitIdx < CIPHER_BLOCK_HEADER_FORMAT_SIZE; digitIdx++)
                        {
                            if (!isdigit((unsigned char)headerZ[CIPHER_BLOCK_HEADER_MAGIC_SIZE + digitIdx]))
                                THROW(FormatError, "invalid cipher header");
                        }

                        format = cvtZSubNToUInt(headerZ, CIPHER_BLOCK_HEADER_MAGIC_SIZE, CIPHER_BLOCK_HEADER_FORMAT_SIZE);

                        // Error on a format this version cannot read before anything is decrypted
                        repoFormatValidate(format);

                        // The format is one this version can read, so the reserved byte must be the value this version writes
                        if (headerZ[CIPHER_BLOCK_MAGIC_SIZE - 1] != CIPHER_BLOCK_HEADER_RESERVED)
                            THROW(FormatError, "invalid cipher header");
                    }
                    // Else the bytes must be the magic, since that is all a file with no header can begin with
                    else if (memcmp(headerZ, CIPHER_BLOCK_MAGIC, CIPHER_BLOCK_MAGIC_SIZE) != 0)
                        THROW(CryptoError, "cipher header invalid");

                    // Error when the format the caller expected is not the one the file was written with
                    if (this->format != 0 && this->format != format)
                        THROW_FMT(FormatError, "expected repository format %u but found %u", this->format, format);

                    this->format = format;
                }
                // Else the first bytes of the file to decrypt should be equal to the magic. If not then this is not an encrypted
                // file, or at least not in a format we recognize.
                else if (!this->raw && memcmp(this->header, CIPHER_BLOCK_MAGIC, CIPHER_BLOCK_MAGIC_SIZE) != 0)
                    THROW(CryptoError, "cipher header invalid");
            }
            // Else copy what was provided into the header buffer and return 0
            else
            {
                memcpy(this->header + this->headerSize, source, sourceSize);
                this->headerSize += sourceSize;

                // Indicate that there is nothing left to process
                sourceSize = 0;
            }
        }

        // If salt generation/read is done
        if (salt)
        {
            // Resolve the digest now that the format is known, which for a header that was read is only true here
            if (this->digest == NULL)
                this->digest = cipherBlockDigest(repoFormatDigest(this->format));

            // Generate key and initialization vector
            uint8_t key[EVP_MAX_KEY_LENGTH];
            uint8_t initVector[EVP_MAX_IV_LENGTH];

            EVP_BytesToKey(this->cipher, this->digest, salt, bufPtrConst(this->pass), (int)bufSize(this->pass), 1, key, initVector);

            // Create context to track cipher
            cryptoError(!(this->cipherContext = EVP_CIPHER_CTX_new()), "unable to create context");

            // Set free callback to ensure cipher context is freed
            memContextCallbackSet(objMemContext(this), cipherBlockFreeResource, this);

            // Initialize cipher
            cryptoError(
                !EVP_CipherInit_ex(this->cipherContext, this->cipher, NULL, key, initVector, this->mode == cipherModeEncrypt),
                "unable to initialize cipher");

            this->saltDone = true;
        }
    }

    // Recheck that source size > 0 as the bytes may have been consumed reading the header
    if (sourceSize > 0)
    {
        // Process the data
        int destinationUpdateSize = 0;

        cryptoError(
            !EVP_CipherUpdate(this->cipherContext, destination, &destinationUpdateSize, source, (int)sourceSize),
            "unable to process cipher");

        destinationSize += (size_t)destinationUpdateSize;

        // Note that data has been processed so flush is valid
        this->processDone = true;
    }

    // Return actual destination size
    FUNCTION_LOG_RETURN(SIZE, destinationSize);
}

/***********************************************************************************************************************************
Flush the remaining data
***********************************************************************************************************************************/
static size_t
cipherBlockFlush(CipherBlock *const this, Buffer *const destination)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(CIPHER_BLOCK, this);
        FUNCTION_LOG_PARAM(BUFFER, destination);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(destination != NULL);

    // Actual destination size
    int destinationSize = 0;

    // If no header was processed then error
    if (!this->saltDone)
        THROW(CryptoError, "cipher header missing");

    // Only flush remaining data if some data was processed
    if (!EVP_CipherFinal(this->cipherContext, bufRemainsPtr(destination), &destinationSize))
        THROW(CryptoError, "unable to flush");

    // Return actual destination size
    FUNCTION_LOG_RETURN(SIZE, (size_t)destinationSize);
}

/***********************************************************************************************************************************
Process function used by C filter
***********************************************************************************************************************************/
static void
cipherBlockProcess(THIS_VOID, const Buffer *const source, Buffer *const destination)
{
    THIS(CipherBlock);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(CIPHER_BLOCK, this);
        FUNCTION_LOG_PARAM(BUFFER, source);
        FUNCTION_LOG_PARAM(BUFFER, destination);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(destination != NULL);
    ASSERT(bufRemains(destination) > 0);

    // Copy already buffered bytes
    if (this->buffer != NULL && !bufEmpty(this->buffer))
    {
        if (bufRemains(destination) >= bufUsed(this->buffer))
        {
            bufCat(destination, this->buffer);
            bufUsedZero(this->buffer);

            this->inputSame = false;
        }
        else
        {
            const size_t catSize = bufRemains(destination);
            bufCatSub(destination, this->buffer, 0, catSize);

            memmove(bufPtr(this->buffer), bufPtr(this->buffer) + catSize, bufUsed(this->buffer) - catSize);
            bufUsedSet(this->buffer, bufUsed(this->buffer) - catSize);

            this->inputSame = true;
        }
    }
    else
    {
        ASSERT(this->buffer == NULL || bufEmpty(this->buffer));

        // Determine how much space is required in the output buffer
        Buffer *outputActual = destination;

        const size_t destinationSize = cipherBlockProcessSize(this, source == NULL ? 0 : bufUsed(source));

        if (destinationSize > bufRemains(destination))
        {
            // Allocate the buffer if needed
            MEM_CONTEXT_OBJ_BEGIN(this)
            {
                if (this->buffer == NULL)
                {
                    this->buffer = bufNew(destinationSize);
                }
                // Resize buffer if needed
                else
                    bufResize(this->buffer, destinationSize);
            }
            MEM_CONTEXT_OBJ_END();

            outputActual = this->buffer;
        }

        // Encrypt/decrypt bytes
        size_t destinationSizeActual;

        if (source == NULL)
        {
            // If salt was not generated it means that process() was never called with any data. It's OK to encrypt a zero byte file
            // but we need to call process to generate the header.
            if (!this->saltDone)
            {
                destinationSizeActual = cipherBlockProcessBlock(this, NULL, 0, bufRemainsPtr(outputActual));
                bufUsedInc(outputActual, destinationSizeActual);
            }

            destinationSizeActual = cipherBlockFlush(this, outputActual);
            this->done = true;
        }
        else
        {
            destinationSizeActual = cipherBlockProcessBlock(
                this, bufPtrConst(source), bufUsed(source), bufRemainsPtr(outputActual));
        }

        bufUsedInc(outputActual, destinationSizeActual);

        // Copy from buffer to destination if needed
        if (this->buffer != NULL && !bufEmpty(this->buffer))
            cipherBlockProcess(this, source, destination);
    }

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Is cipher done?
***********************************************************************************************************************************/
static bool
cipherBlockDone(const THIS_VOID)
{
    THIS(const CipherBlock);

    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(CIPHER_BLOCK, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(BOOL, this->done && !this->inputSame);
}

/***********************************************************************************************************************************
Should the same input be provided again?
***********************************************************************************************************************************/
static bool
cipherBlockInputSame(const THIS_VOID)
{
    THIS(const CipherBlock);

    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(CIPHER_BLOCK, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(BOOL, this->inputSame);
}

/***********************************************************************************************************************************
Report the format the header contained
***********************************************************************************************************************************/
static Pack *
cipherBlockResult(THIS_VOID)
{
    THIS(CipherBlock);

    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(CIPHER_BLOCK, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    Pack *result = NULL;

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
FN_EXTERN unsigned int
cipherBlockFormat(PackRead *const cipherBlockResult)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PACK_READ, cipherBlockResult);
    FUNCTION_TEST_END();

    ASSERT(cipherBlockResult != NULL);

    FUNCTION_TEST_RETURN(UINT, pckReadU32P(cipherBlockResult));
}

/**********************************************************************************************************************************/
FN_EXTERN IoFilter *
cipherBlockNew(const CipherMode mode, const CipherSpec *const cipherSpec, const CipherBlockNewParam param)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STRING_ID, mode);
        FUNCTION_LOG_PARAM(CIPHER_SPEC, cipherSpec);
        FUNCTION_LOG_PARAM(BOOL, param.raw);
        FUNCTION_LOG_PARAM(BOOL, param.header);
        FUNCTION_LOG_PARAM(UINT, param.format);
    FUNCTION_LOG_END();

    ASSERT(cipherSpec != NULL);
    ASSERT(cipherSpecType(cipherSpec) != cipherTypeNone);
    ASSERT(cipherSpecPass(cipherSpec) != NULL && !bufEmpty(cipherSpecPass(cipherSpec)));

    // The header takes the place of the magic, so a file that contains one is never also raw
    ASSERT(!param.raw || (!param.header && param.format == 0));

    // On encrypt the format defines whether a header is written, so a header is only ever requested on decrypt
    ASSERT(mode == cipherModeDecrypt || !param.header);

    // Init crypto subsystem
    cryptoInit();

    // Lookup cipher by name. This means the ciphers passed in must exactly match a name expected by OpenSSL. This is a good thing
    // since the name required by the openssl command-line tool will match what is used by pgBackRest.
    char *const cipherTypeZ = zNewStrId(cipherSpecType(cipherSpec));
    const EVP_CIPHER *cipher = EVP_get_cipherbyname(cipherTypeZ);

    if (!cipher)
        THROW_FMT(AssertError, "unable to load cipher '%s'", cipherTypeZ);

    zFree(cipherTypeZ);

    // Lookup digest. A header that has yet to be read is what defines the format of the file, and the format defines the digest, so
    // in that case the lookup waits until the header has been read.
    const EVP_MD *digest = NULL;

    if (!param.header)
    {
        // A format defines the digest, otherwise it comes from the spec
        digest = cipherBlockDigest(param.format != 0 ? repoFormatDigest(param.format) : cipherSpecDigest(cipherSpec));
    }

    OBJ_NEW_BEGIN(CipherBlock, .childQty = MEM_CONTEXT_QTY_MAX, .callbackQty = 1)
    {
        *this = (CipherBlock)
        {
            .mode = mode,
            .raw = param.raw,
            .headerFormat = mode == cipherModeEncrypt ? param.format >= REPOSITORY_FORMAT_6 : param.header,
            .format = param.format,
            .cipher = cipher,
            .digest = digest,
            .pass = bufDup(cipherSpecPass(cipherSpec)),
        };
    }
    OBJ_NEW_END();

    // Create param list
    Pack *paramList;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        PackWrite *const packWrite = pckWriteNewP();

        pckWriteU64P(packWrite, mode);
        cipherSpecPack(packWrite, cipherSpec);
        pckWriteBoolP(packWrite, param.raw);
        pckWriteBoolP(packWrite, param.header);
        pckWriteU32P(packWrite, param.format);
        pckWriteEndP(packWrite);

        paramList = pckMove(pckWriteResult(packWrite), memContextPrior());
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(
        IO_FILTER,
        ioFilterNewP(
            CIPHER_BLOCK_FILTER_TYPE, this, paramList, .done = cipherBlockDone, .inOut = cipherBlockProcess,
            .inputSame = cipherBlockInputSame,

            // Only a filter that reads a header has a format to report
            .result = param.header ? cipherBlockResult : NULL));
}

FN_EXTERN IoFilter *
cipherBlockNewPack(const Pack *const paramList)
{
    IoFilter *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        PackRead *const paramListPack = pckReadNew(paramList);
        const CipherMode cipherMode = (CipherMode)pckReadU64P(paramListPack);
        const CipherSpec *const cipherSpec = cipherSpecNewPack(paramListPack);
        const bool raw = pckReadBoolP(paramListPack);
        const bool header = pckReadBoolP(paramListPack);
        const unsigned int format = pckReadU32P(paramListPack);

        result = ioFilterMove(
            cipherBlockNewP(cipherMode, cipherSpec, .raw = raw, .header = header, .format = format), memContextPrior());
    }
    MEM_CONTEXT_TEMP_END();

    return result;
}

/**********************************************************************************************************************************/
FN_EXTERN IoFilterGroup *
cipherBlockFilterGroupAdd(
    IoFilterGroup *const filterGroup, const CipherMode mode, const CipherSpec *const cipherSpec,
    const CipherBlockFilterGroupAddParam param)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(IO_FILTER_GROUP, filterGroup);
        FUNCTION_LOG_PARAM(STRING_ID, mode);
        FUNCTION_LOG_PARAM(CIPHER_SPEC, cipherSpec);
        FUNCTION_LOG_PARAM(UINT, param.format);
    FUNCTION_LOG_END();

    ASSERT(filterGroup != NULL);
    ASSERT(cipherSpec != NULL);

    if (cipherSpecType(cipherSpec) != cipherTypeNone)
        ioFilterGroupAdd(filterGroup, cipherBlockNewP(mode, cipherSpec, .format = param.format));

    FUNCTION_LOG_RETURN(IO_FILTER_GROUP, filterGroup);
}
