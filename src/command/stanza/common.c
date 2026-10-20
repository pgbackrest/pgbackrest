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

/**********************************************************************************************************************************/
FN_EXTERN void
cipherRoundCheck(const unsigned int repoIdx, const unsigned int format)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(UINT, repoIdx);
        FUNCTION_TEST_PARAM(UINT, format);
    FUNCTION_TEST_END();

    if (format < REPOSITORY_FORMAT_6 && cfgOptionIdxSource(cfgOptRepoCipherRound, repoIdx) != cfgSourceDefault)
    {
        LOG_WARN_FMT(
            "option '%s' requires repository format %d or higher", cfgOptionIdxName(cfgOptRepoCipherRound, repoIdx),
            REPOSITORY_FORMAT_6);
    }

    FUNCTION_TEST_RETURN_VOID();
}

/***********************************************************************************************************************************
Does the passphrase look generated? It must be base64 that decodes to at least 256 bits, the decoded bytes must not all be text, and
there must be enough distinct byte values and enough distinct differences between adjacent bytes. Too few distinct differences
means repeated values, a sequence, or a short repeating pattern. 32 random bytes are all text with a probability of about 10^-14
and have fewer than 16 distinct values or differences with a probability of about 10^-15. The passphrase could still be a weak
secret that was hashed or encoded some other way, which we can't detect.
***********************************************************************************************************************************/
// Bytes the key must decode to, i.e. 256 bits
#define CIPHER_PASS_GENERATED_SIZE_MIN                              32

// Distinct byte values and distinct differences between adjacent bytes the key must contain
#define CIPHER_PASS_GENERATED_DISTINCT_MIN                          16

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
        TRY_BEGIN()
        {
            const Buffer *const key = bufNewDecode(encodingBase64, strNewBuf(cipherSpecPass(cipherSpec)));

            if (bufUsed(key) >= CIPHER_PASS_GENERATED_SIZE_MIN)
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
        // A passphrase that is not base64 does not look generated
        CATCH(FormatError)
        {
        }
        TRY_END();
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_TEST_RETURN(BOOL, result);
}

/**********************************************************************************************************************************/
FN_EXTERN unsigned int
cipherRoundGet(const unsigned int repoIdx)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(UINT, repoIdx);
    FUNCTION_TEST_END();

    unsigned int result = cfgOptionIdxUInt(cfgOptRepoCipherRound, repoIdx);

    // One round when the rounds are default and the passphrase looks generated
    if (cfgOptionIdxSource(cfgOptRepoCipherRound, repoIdx) == cfgSourceDefault &&
        cipherPassGenerated(cfgCipherSpecMainIdx(repoIdx)))
    {
        result = 1;
    }

    FUNCTION_TEST_RETURN(UINT, result);
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
