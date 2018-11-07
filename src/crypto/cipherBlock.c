/***********************************************************************************************************************************
Block Cipher
***********************************************************************************************************************************/
#include <string.h>

#include <openssl/evp.h>
#include <openssl/err.h>

#include "common/debug.h"
#include "common/log.h"
#include "common/memContext.h"
#include "crypto/cipherBlock.h"
#include "crypto/crypto.h"
#include "crypto/random.h"

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
Track state during block encrypt/decrypt
***********************************************************************************************************************************/
struct CipherBlock
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
};

/***********************************************************************************************************************************
New block encrypt/decrypt object
***********************************************************************************************************************************/
CipherBlock *
cipherBlockNew(CipherMode mode, const char *cipherName, const unsigned char *pass, size_t passSize, const char *digestName)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(ENUM, mode);
        FUNCTION_DEBUG_PARAM(STRINGZ, cipherName);
        FUNCTION_DEBUG_PARAM(UCHARP, pass);
        FUNCTION_DEBUG_PARAM(SIZE, passSize);
        FUNCTION_DEBUG_PARAM(STRINGZ, digestName);

        FUNCTION_DEBUG_ASSERT(cipherName != NULL);
        FUNCTION_DEBUG_ASSERT(pass != NULL);
        FUNCTION_DEBUG_ASSERT(passSize > 0);
    FUNCTION_DEBUG_END();

    // Only need to init once.
    if (!cryptoIsInit())
        cryptoInit();

    // Lookup cipher by name.  This means the ciphers passed in must exactly match a name expected by OpenSSL.  This is a good
    // thing since the name required by the openssl command-line tool will match what is used by pgBackRest.
    const EVP_CIPHER *cipher = EVP_get_cipherbyname(cipherName);

    if (!cipher)
        THROW_FMT(AssertError, "unable to load cipher '%s'", cipherName);

    // Lookup digest.  If not defined it will be set to sha1.
    const EVP_MD *digest = NULL;

    if (digestName)
        digest = EVP_get_digestbyname(digestName);
    else
        digest = EVP_sha1();

    if (!digest)
        THROW_FMT(AssertError, "unable to load digest '%s'", digestName);

    // Allocate memory to hold process state
    CipherBlock *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("cipherBlock")
    {
        // Allocate state and set context
        this = memNew(sizeof(CipherBlock));
        this->memContext = MEM_CONTEXT_NEW();

        // Set mode, encrypt or decrypt
        this->mode = mode;

        // Set cipher and digest
        this->cipher = cipher;
        this->digest = digest;

        // Store the passphrase
        this->passSize = passSize;
        this->pass = memNewRaw(this->passSize);
        memcpy(this->pass, pass, this->passSize);
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_DEBUG_RESULT(CIPHER_BLOCK, this);
}

/***********************************************************************************************************************************
Determine how large the destination buffer should be
***********************************************************************************************************************************/
size_t
cipherBlockProcessSize(CipherBlock *this, size_t sourceSize)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(CIPHER_BLOCK, this);
        FUNCTION_DEBUG_PARAM(SIZE, sourceSize);

        FUNCTION_DEBUG_ASSERT(this != NULL);
    FUNCTION_DEBUG_END();

    // Destination size is source size plus one extra block
    size_t destinationSize = sourceSize + EVP_MAX_BLOCK_LENGTH;

    // On encrypt the header size must be included before the first block
    if (this->mode == cipherModeEncrypt && !this->saltDone)
        destinationSize += CIPHER_BLOCK_MAGIC_SIZE + PKCS5_SALT_LEN;

    FUNCTION_DEBUG_RESULT(SIZE, destinationSize);
}

/***********************************************************************************************************************************
Encrypt/decrypt data
***********************************************************************************************************************************/
size_t
cipherBlockProcess(CipherBlock *this, const unsigned char *source, size_t sourceSize, unsigned char *destination)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(CIPHER_BLOCK, this);
        FUNCTION_DEBUG_PARAM(UCHARP, source);
        FUNCTION_DEBUG_PARAM(SIZE, sourceSize);
        FUNCTION_DEBUG_PARAM(UCHARP, destination);

        FUNCTION_DEBUG_ASSERT(this != NULL);
        FUNCTION_DEBUG_ASSERT(source != NULL);
        FUNCTION_DEBUG_ASSERT(destination != NULL);
    FUNCTION_DEBUG_END();

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
            randomBytes(destination, PKCS5_SALT_LEN);
            salt = destination;
            destination += PKCS5_SALT_LEN;
            destinationSize += PKCS5_SALT_LEN;
        }
        // On decrypt the salt is read from the header
        else
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
                    THROW(CipherError, "cipher header invalid");
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
            memContextCallback(this->memContext, (MemContextCallback)cipherBlockFree, this);

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
    FUNCTION_DEBUG_RESULT(SIZE, destinationSize);
}

/***********************************************************************************************************************************
Flush the remaining data
***********************************************************************************************************************************/
size_t
cipherBlockFlush(CipherBlock *this, unsigned char *destination)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(CIPHER_BLOCK, this);
        FUNCTION_DEBUG_PARAM(UCHARP, destination);

        FUNCTION_DEBUG_ASSERT(this != NULL);
        FUNCTION_DEBUG_ASSERT(destination != NULL);
    FUNCTION_DEBUG_END();

    // Actual destination size
    size_t destinationSize = 0;

    // If no header was processed then error
    if (!this->saltDone)
        THROW(CipherError, "cipher header missing");

    // Only flush remaining data if some data was processed
    if (!EVP_CipherFinal(this->cipherContext, destination, (int *)&destinationSize))
        THROW(CipherError, "unable to flush");

    // Return actual destination size
    FUNCTION_DEBUG_RESULT(SIZE, destinationSize);
}

/***********************************************************************************************************************************
Free memory
***********************************************************************************************************************************/
void
cipherBlockFree(CipherBlock *this)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(CIPHER_BLOCK, this);

        FUNCTION_DEBUG_ASSERT(this != NULL);
    FUNCTION_DEBUG_END();

    // Free cipher context
    if (this->cipherContext)
        EVP_CIPHER_CTX_cleanup(this->cipherContext);

    // Free mem context
    memContextFree(this->memContext);

    FUNCTION_DEBUG_RESULT_VOID();
}
