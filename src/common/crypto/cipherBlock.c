/***********************************************************************************************************************************
Block Cipher
***********************************************************************************************************************************/
#include "build.auto.h"

#include <string.h>

#include <openssl/evp.h>
#include <openssl/err.h>

#include "common/crypto/cipherBlock.h"
#include "common/crypto/common.h"
#include "common/debug.h"
#include "common/io/filter/filter.intern.h"
#include "common/log.h"
#include "common/memContext.h"
#include "common/type/object.h"

/***********************************************************************************************************************************
Filter type constant
***********************************************************************************************************************************/
STRING_EXTERN(CIPHER_BLOCK_FILTER_TYPE_STR,                         CIPHER_BLOCK_FILTER_TYPE);

/***********************************************************************************************************************************
Header constants and sizes
***********************************************************************************************************************************/
// Magic constant for salted encrypt.  Only salted encrypt is done here, but this constant is required for compatibility with the
// openssl command-line tool.
#define CIPHER_BLOCK_MAGIC                                          "Salted__"
#define CIPHER_BLOCK_MAGIC_SIZE                                     (sizeof(CIPHER_BLOCK_MAGIC) - 1)

// Total length of cipher header
#define CIPHER_BLOCK_HEADER_SIZE                                    (CIPHER_BLOCK_MAGIC_SIZE + PKCS5_SALT_LEN)

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
#define CIPHER_BLOCK_TYPE                                           CipherBlock
#define CIPHER_BLOCK_PREFIX                                         cipherBlock

typedef struct CipherBlock
{
    MemContext *memContext;                                         // Context to store data
    CipherMode mode;                                                // Mode encrypt/decrypt
    bool saltDone;                                                  // Has the salt been read/generated?
    bool processDone;                                               // Has any data been processed?
    size_t passSize;                                                // Size of passphrase in bytes
    unsigned char *pass;                                            // Passphrase used to generate encryption key
    size_t headerSize;                                              // Size of header read during decrypt
    unsigned char header[CIPHER_BLOCK_HEADER_SIZE];                 // Buffer to hold partial header during decrypt
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
String *
cipherBlockToLog(const CipherBlock *this)
{
    return strNewFmt(
        "{inputSame: %s, done: %s}", cvtBoolToConstZ(this->inputSame), cvtBoolToConstZ(this->done));
}

#define FUNCTION_LOG_CIPHER_BLOCK_TYPE                                                                                             \
    CipherBlock *
#define FUNCTION_LOG_CIPHER_BLOCK_FORMAT(value, buffer, bufferSize)                                                                \
    FUNCTION_LOG_STRING_OBJECT_FORMAT(value, cipherBlockToLog, buffer, bufferSize)

/***********************************************************************************************************************************
Free cipher context
***********************************************************************************************************************************/
OBJECT_DEFINE_FREE_RESOURCE_BEGIN(CIPHER_BLOCK, LOG, logLevelTrace)
{
    EVP_CIPHER_CTX_free(this->cipherContext);
}
OBJECT_DEFINE_FREE_RESOURCE_END(LOG);

/***********************************************************************************************************************************
Determine how large the destination buffer should be
***********************************************************************************************************************************/
static size_t
cipherBlockProcessSize(CipherBlock *this, size_t sourceSize)
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
cipherBlockProcessBlock(CipherBlock *this, const unsigned char *source, size_t sourceSize, unsigned char *destination)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(CIPHER_BLOCK, this);
        FUNCTION_LOG_PARAM_P(UCHARDATA, source);
        FUNCTION_LOG_PARAM(SIZE, sourceSize);
        FUNCTION_LOG_PARAM_P(UCHARDATA, destination);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(source != NULL || sourceSize == 0);
    ASSERT(destination != NULL);

    // Actual destination size
    size_t destinationSize = 0;

    // If the salt has not been generated/read yet
    if (!this->saltDone)
    {
        const unsigned char *salt = NULL;

        // On encrypt the salt is generated
        if (this->mode == cipherModeEncrypt)
        {
            // Add magic to the destination buffer so openssl knows the file is salted
            memcpy(destination, CIPHER_BLOCK_MAGIC, CIPHER_BLOCK_MAGIC_SIZE);
            destination += CIPHER_BLOCK_MAGIC_SIZE;
            destinationSize += CIPHER_BLOCK_MAGIC_SIZE;

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
            if (this->headerSize + sourceSize >= CIPHER_BLOCK_HEADER_SIZE)
            {
                // Copy header (or remains of header) from source into the header buffer
                memcpy(this->header + this->headerSize, source, CIPHER_BLOCK_HEADER_SIZE - this->headerSize);
                salt = this->header + CIPHER_BLOCK_MAGIC_SIZE;

                // Advance source and source size by the number of bytes read
                source += CIPHER_BLOCK_HEADER_SIZE - this->headerSize;
                sourceSize -= CIPHER_BLOCK_HEADER_SIZE - this->headerSize;

                // The first bytes of the file to decrypt should be equal to the magic.  If not then this is not an
                // encrypted file, or at least not in a format we recognize.
                if (memcmp(this->header, CIPHER_BLOCK_MAGIC, CIPHER_BLOCK_MAGIC_SIZE) != 0)
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
            // Generate key and initialization vector
            unsigned char key[EVP_MAX_KEY_LENGTH];
            unsigned char initVector[EVP_MAX_IV_LENGTH];

            EVP_BytesToKey(
                this->cipher, this->digest, salt, (unsigned char *)this->pass, (int)this->passSize, 1, key, initVector);

            // Create context to track cipher
            cryptoError(!(this->cipherContext = EVP_CIPHER_CTX_new()), "unable to create context");

            // Set free callback to ensure cipher context is freed
            memContextCallbackSet(this->memContext, cipherBlockFreeResource, this);

            // Initialize cipher
            cryptoError(
                !EVP_CipherInit_ex(
                    this->cipherContext, this->cipher, NULL, key, initVector, this->mode == cipherModeEncrypt),
                    "unable to initialize cipher");

            this->saltDone = true;
        }
    }

    // Recheck that source size > 0 as the bytes may have been consumed reading the header
    if (sourceSize > 0)
    {
        // Process the data
        size_t destinationUpdateSize = 0;

        cryptoError(
            !EVP_CipherUpdate(this->cipherContext, destination, (int *)&destinationUpdateSize, source, (int)sourceSize),
            "unable to process cipher");

        destinationSize += destinationUpdateSize;

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
cipherBlockFlush(CipherBlock *this, Buffer *destination)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(CIPHER_BLOCK, this);
        FUNCTION_LOG_PARAM(BUFFER, destination);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(destination != NULL);

    // Actual destination size
    size_t destinationSize = 0;

    // If no header was processed then error
    if (!this->saltDone)
        THROW(CryptoError, "cipher header missing");

    // Only flush remaining data if some data was processed
    if (!EVP_CipherFinal(this->cipherContext, bufRemainsPtr(destination), (int *)&destinationSize))
        THROW(CryptoError, "unable to flush");

    // Return actual destination size
    FUNCTION_LOG_RETURN(SIZE, destinationSize);
}

/***********************************************************************************************************************************
Process function used by C filter
***********************************************************************************************************************************/
static void
cipherBlockProcess(THIS_VOID, const Buffer *source, Buffer *destination)
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
    if (this->buffer != NULL && bufUsed(this->buffer) > 0)
    {
        if (bufRemains(destination) >= bufUsed(this->buffer))
        {
            bufCat(destination, this->buffer);
            bufUsedZero(this->buffer);

            this->inputSame = false;
        }
        else
        {
            size_t catSize = bufRemains(destination);
            bufCatSub(destination, this->buffer, 0, catSize);

            memmove(bufPtr(this->buffer), bufPtr(this->buffer) + catSize, bufUsed(this->buffer) - catSize);
            bufUsedSet(this->buffer, bufUsed(this->buffer) - catSize);

            this->inputSame = true;
        }
    }
    else
    {
        ASSERT(this->buffer == NULL || bufUsed(this->buffer) == 0);

        // Determine how much space is required in the output buffer
        Buffer *outputActual = destination;

        size_t destinationSize = cipherBlockProcessSize(this, source == NULL ? 0 : bufUsed(source));

        if (destinationSize > bufRemains(destination))
        {
            // Allocate the buffer if needed
            MEM_CONTEXT_BEGIN(this->memContext)
            {
                if (this->buffer == NULL)
                {
                    this->buffer = bufNew(destinationSize);
                }
                // Resize buffer if needed
                else
                    bufResize(this->buffer, destinationSize);
            }
            MEM_CONTEXT_END();

            outputActual = this->buffer;
        }

        // Encrypt/decrypt bytes
        size_t destinationSizeActual;

        if (source == NULL)
        {
            // If salt was not generated it means that process() was never called with any data.  It's OK to encrypt a zero byte
            // file but we need to call process to generate the header.
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
        if (this->buffer != NULL && bufUsed(this->buffer) > 0)
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

    FUNCTION_TEST_RETURN(this->done && !this->inputSame);
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

    FUNCTION_TEST_RETURN(this->inputSame);
}

/**********************************************************************************************************************************/
IoFilter *
cipherBlockNew(CipherMode mode, CipherType cipherType, const Buffer *pass, const String *digestName)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(ENUM, mode);
        FUNCTION_LOG_PARAM(ENUM, cipherType);
        FUNCTION_TEST_PARAM(BUFFER, pass);                          // Use FUNCTION_TEST so passphrase is not logged
        FUNCTION_LOG_PARAM(STRING, digestName);
    FUNCTION_LOG_END();

    ASSERT(pass != NULL);
    ASSERT(bufSize(pass) > 0);

    // Init crypto subsystem
    cryptoInit();

    // Lookup cipher by name.  This means the ciphers passed in must exactly match a name expected by OpenSSL.  This is a good
    // thing since the name required by the openssl command-line tool will match what is used by pgBackRest.
    const EVP_CIPHER *cipher = EVP_get_cipherbyname(strPtr(cipherTypeName(cipherType)));

    if (!cipher)
        THROW_FMT(AssertError, "unable to load cipher '%s'", strPtr(cipherTypeName(cipherType)));

    // Lookup digest.  If not defined it will be set to sha1.
    const EVP_MD *digest = NULL;

    if (digestName)
        digest = EVP_get_digestbyname(strPtr(digestName));
    else
        digest = EVP_sha1();

    if (!digest)
        THROW_FMT(AssertError, "unable to load digest '%s'", strPtr(digestName));

    // Allocate memory to hold process state
    IoFilter *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("CipherBlock")
    {
        CipherBlock *driver = memNew(sizeof(CipherBlock));

        *driver = (CipherBlock)
        {
            .memContext = MEM_CONTEXT_NEW(),
            .mode = mode,
            .cipher = cipher,
            .digest = digest,
            .passSize = bufUsed(pass),
        };

        // Store the passphrase
        driver->pass = memNew(driver->passSize);
        memcpy(driver->pass, bufPtrConst(pass), driver->passSize);

        // Create param list
        VariantList *paramList = varLstNew();
        varLstAdd(paramList, varNewUInt(mode));
        varLstAdd(paramList, varNewUInt(cipherType));
        // ??? Using a string here is not correct since the passphrase is being passed as a buffer so may contain null characters.
        // However, since strings are used to hold the passphrase in the rest of the code this is currently valid.
        varLstAdd(paramList, varNewStr(strNewBuf(pass)));
        varLstAdd(paramList, digestName ? varNewStr(digestName) : NULL);

        // Create filter interface
        this = ioFilterNewP(
            CIPHER_BLOCK_FILTER_TYPE_STR, driver, paramList, .done = cipherBlockDone, .inOut = cipherBlockProcess,
            .inputSame = cipherBlockInputSame);
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_LOG_RETURN(IO_FILTER, this);
}

IoFilter *
cipherBlockNewVar(const VariantList *paramList)
{
    return cipherBlockNew(
        (CipherMode)varUIntForce(varLstGet(paramList, 0)), (CipherType)varUIntForce(varLstGet(paramList, 1)),
        BUFSTR(varStr(varLstGet(paramList, 2))), varLstGet(paramList, 3) == NULL ? NULL : varStr(varLstGet(paramList, 3)));
}

/**********************************************************************************************************************************/
IoFilterGroup *
cipherBlockFilterGroupAdd(IoFilterGroup *filterGroup, CipherType type, CipherMode mode, const String *pass)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(IO_FILTER_GROUP, filterGroup);
        FUNCTION_LOG_PARAM(ENUM, type);
        FUNCTION_LOG_PARAM(ENUM, mode);
        FUNCTION_TEST_PARAM(STRING, pass);                          // Use FUNCTION_TEST so passphrase is not logged
    FUNCTION_LOG_END();

    ASSERT(filterGroup != NULL);
    ASSERT((type == cipherTypeNone && pass == NULL) || (type != cipherTypeNone && pass != NULL));

    if (type != cipherTypeNone)
        ioFilterGroupAdd(filterGroup, cipherBlockNew(mode, type, BUFSTR(pass), NULL));

    FUNCTION_LOG_RETURN(IO_FILTER_GROUP, filterGroup);
}
