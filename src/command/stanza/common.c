/***********************************************************************************************************************************
Stanza Commands Handler
***********************************************************************************************************************************/
#include <build.h>

#include "command/check/common.h"
#include "command/stanza/common.h"
#include "common/debug.h"
#include "common/encode.h"
#include "common/format/format.h"
#include "common/log.h"
#include "config/config.h"
#include "db/helper.h"
#include "info/infoPg.h"
#include "postgres/interface.h"
#include "postgres/version.h"
#include "storage/helper.h"

/**********************************************************************************************************************************/
FN_EXTERN CipherSpec *
cipherSpecGen(const CipherType cipherType, const unsigned int format)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING_ID, cipherType);
        FUNCTION_TEST_PARAM(UINT, format);
    FUNCTION_TEST_END();

    CipherSpec *result;

    if (cipherType == cipherTypeNone)
        result = cipherSpecNewNone();
    else
    {
        MEM_CONTEXT_TEMP_BEGIN()
        {
            Buffer *pass;

            // Format >= 6 uses 256 random bits as the pass
            if (format >= REPOSITORY_FORMAT_6)
            {
                pass = bufNew(32);
                cryptoRandomBytes(bufPtr(pass), bufSize(pass));
                bufUsedSet(pass, bufSize(pass));
            }
            // Else the pass is the base64 text of 48 random bytes. Earlier formats store this text and derive the key from it.
            else
            {
                uint8_t buffer[48];                                 // 48 is the amount of entropy needed to get a 64 base key
                cryptoRandomBytes(buffer, sizeof(buffer));

                const String *const text = strNewEncode(encodingBase64, BUF(buffer, sizeof(buffer)));
                pass = bufNewC(strZ(text), strSize(text));
            }

            // A new pass derives with the digest its format calls for and that digest is stored with it, so a reader derives the
            // same key
            result = cipherSpecNewP(cipherType, pass, .digest = repoFormatDigest(format));
            cipherSpecMove(result, memContextPrior());
        }
        MEM_CONTEXT_TEMP_END();
    }

    FUNCTION_TEST_RETURN(CIPHER_SPEC, result);
}

/***********************************************************************************************************************************
Does the passphrase look randomly generated? It must be hex, base64, or unpadded base64url that decodes to at least 256 bits, the
decoded bytes must not all be text, and there must be enough distinct byte values and enough distinct differences between adjacent
bytes. Too few distinct differences means repeated values, a sequence, or a short repeating pattern. 32 random bytes are all text
with a probability of about 10^-14 and have fewer than 16 distinct values or differences with a probability of about 10^-15. The
passphrase could still be a weak secret that was hashed or encoded some other way, which we can't detect.
***********************************************************************************************************************************/
// Bytes the key must decode to, i.e. 256 bits
#define CIPHER_PASS_GENERATED_SIZE_MIN                              32

// Distinct byte values and distinct differences between adjacent bytes the key must contain
#define CIPHER_PASS_GENERATED_DISTINCT_MIN                          16

// Prefix of the passphrase examples in the documentation
#define CIPHER_PASS_EXAMPLE_PREFIX                                  "zWaf6XtpjIVZC5444yXB+cgFDFl7MxGlgkZSaoPvTGi"

// Encodings to try in order. A hex passphrase may also be valid base64.
static const EncodingType cipherPassGeneratedEncoding[] = {encodingHex, encodingBase64, encodingBase64Url};

static bool
cipherPassGenerated(const CipherSpec *const cipherSpec)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(CIPHER_SPEC, cipherSpec);
    FUNCTION_TEST_END();

    ASSERT(cipherSpec != NULL);
    ASSERT(cipherSpecType(cipherSpec) != cipherTypeNone);

    bool result = false;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        const String *const pass = strNewBuf(cipherSpecPass(cipherSpec));
        const Buffer *key = NULL;

        // Decode with the first encoding the passphrase is valid in
        for (unsigned int encodingIdx = 0; encodingIdx < LENGTH_OF(cipherPassGeneratedEncoding) && key == NULL; encodingIdx++)
        {
            TRY_BEGIN()
            {
                key = bufNewDecode(cipherPassGeneratedEncoding[encodingIdx], pass);
            }
            // The passphrase is not valid in this encoding
            CATCH(FormatError)
            {
            }
            TRY_END();
        }

        if (key != NULL && bufUsed(key) >= CIPHER_PASS_GENERATED_SIZE_MIN)
        {
            bool text = true;
            bool byteFound[256] = {false};
            unsigned int byteDistinct = 0;
            bool deltaFound[256] = {false};
            unsigned int deltaDistinct = 0;

            for (size_t keyIdx = 0; keyIdx < bufUsed(key); keyIdx++)
            {
                const uint8_t byte = bufPtrConst(key)[keyIdx];

                // A byte that is neither printable ASCII nor a tab, LF, or CR means the key is not text
                if ((byte < ' ' || byte > '~') && byte != '\t' && byte != '\n' && byte != '\r')
                    text = false;

                // Count each byte value the first time it is seen
                if (!byteFound[byte])
                {
                    byteFound[byte] = true;
                    byteDistinct++;
                }

                // Count each difference from the prior byte the first time it is seen
                if (keyIdx > 0)
                {
                    const uint8_t delta = (uint8_t)(byte - bufPtrConst(key)[keyIdx - 1]);

                    if (!deltaFound[delta])
                    {
                        deltaFound[delta] = true;
                        deltaDistinct++;
                    }
                }
            }

            result =
                !text && byteDistinct >= CIPHER_PASS_GENERATED_DISTINCT_MIN &&
                deltaDistinct >= CIPHER_PASS_GENERATED_DISTINCT_MIN;
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_TEST_RETURN(BOOL, result);
}

/**********************************************************************************************************************************/
FN_EXTERN void
stanzaCipherPassCheck(const unsigned int repoIdx, const unsigned int format)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(UINT, repoIdx);
        FUNCTION_TEST_PARAM(UINT, format);
    FUNCTION_TEST_END();

    if (cfgOptionIdxStrId(cfgOptRepoCipherType, repoIdx) != cipherTypeNone)
    {
        // Error when the passphrase is a documentation example
        if (strBeginsWithZ(cfgOptionIdxStr(cfgOptRepoCipherPass, repoIdx), CIPHER_PASS_EXAMPLE_PREFIX))
        {
            THROW_FMT(
                OptionInvalidValueError,
                "option '%s' must not be the example passphrase from the documentation\n"
                "HINT: generate a passphrase with 'openssl rand -base64 32'.",
                cfgOptionIdxName(cfgOptRepoCipherPass, repoIdx));
        }

        // Format >= 6 requires the passphrase to look generated
        if (format >= REPOSITORY_FORMAT_6 && !cipherPassGenerated(cfgCipherSpecMainIdx(repoIdx)))
        {
            THROW_FMT(
                OptionInvalidValueError,
                "option '%s' must be at least 256 random bits encoded as hex, base64, or base64url to create a stanza at repository"
                " format " STRINGIFY(REPOSITORY_FORMAT_6) " or higher\n"
                "HINT: generate a passphrase with 'openssl rand -base64 32'.",
                cfgOptionIdxName(cfgOptRepoCipherPass, repoIdx));
        }
    }

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
FN_EXTERN PgControl
pgValidate(void)
{
    FUNCTION_TEST_VOID();

    PgControl result = {0};

    MEM_CONTEXT_TEMP_BEGIN()
    {
        if (cfgOptionBool(cfgOptOnline))
        {
            // Check the primary connections (and standby, if any) and return the primary database object.
            const DbGetResult dbObject = dbGet(false, true, CFGOPTVAL_BACKUP_STANDBY_N);

            // Get the pgControl information from the pg*-path deemed to be the primary
            result = dbPgControl(dbObject.primary);

            // Check the user configured path and version against the database
            checkDbConfig(dbPgControl(dbObject.primary).version, dbObject.primaryIdx, dbObject.primary, false);
        }
        // If the database is not online, assume that pg1 is the primary
        else
            result = pgControlFromFile(storagePg(), cfgOptionStrNull(cfgOptPgVersionForce));
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_TEST_RETURN_TYPE(PgControl, result);
}
