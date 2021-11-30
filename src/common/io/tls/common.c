/***********************************************************************************************************************************
TLS Common
***********************************************************************************************************************************/
#include "build.auto.h"

#include <string.h>
#include <sys/stat.h>

#include <openssl/ssl.h>

#include "common/crypto/common.h"
#include "common/debug.h"
#include "common/io/tls/common.h"
#include "common/user.h"
#include "storage/posix/storage.h"

/**********************************************************************************************************************************/
String *
tlsAsn1ToStr(ASN1_STRING *const nameAsn1)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM_P(VOID, nameAsn1);
    FUNCTION_TEST_END();

    // The name should not be null
    if (nameAsn1 == NULL)                                                                                           // {vm_covered}
        THROW(CryptoError, "TLS certificate name entry is missing");

    FUNCTION_TEST_RETURN(                                                                                           // {vm_covered}
        strNewZN(
#if OPENSSL_VERSION_NUMBER < 0x10100000L
            (const char *)ASN1_STRING_data(nameAsn1),
#else
            (const char *)ASN1_STRING_get0_data(nameAsn1),
#endif
            (size_t)ASN1_STRING_length(nameAsn1)));
}

/**********************************************************************************************************************************/
void
tlsCertNameVerify(const String *const name)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, name);
    FUNCTION_TEST_END();

    // Check for embedded NULLs
    if (strlen(strZ(name)) != strSize(name))
        THROW(CryptoError, "TLS certificate name contains embedded null");

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
String *
tlsCertCommonName(X509 *const certificate)                                                                          // {vm_covered}
{
    FUNCTION_TEST_BEGIN();                                                                                          // {vm_covered}
        FUNCTION_TEST_PARAM_P(VOID, certificate);                                                                   // {vm_covered}
    FUNCTION_TEST_END();                                                                                            // {vm_covered}

    X509_NAME *const subjectName = X509_get_subject_name(certificate);                                              // {vm_covered}
    CHECK(FormatError, subjectName != NULL, "subject name is missing");                                             // {vm_covered}

    const int commonNameIndex = X509_NAME_get_index_by_NID(subjectName, NID_commonName, -1);                        // {vm_covered}
    CHECK(FormatError, commonNameIndex >= 0, "common name is missing");                                             // {vm_covered}

    String *result = tlsAsn1ToStr(X509_NAME_ENTRY_get_data(X509_NAME_get_entry(subjectName, commonNameIndex)));     // {vm_covered}

    // Check for NULLs in the name
    tlsCertNameVerify(result);                                                                                      // {vm_covered}

    FUNCTION_TEST_RETURN(result);                                                                                   // {vm_covered}
}

/**********************************************************************************************************************************/
// Callback to process cert passwords
static int
tlsCertPwd(char *buffer, const int size, const int rwFlag, void *const userData)
{
    CHECK(ServiceError, size > 0, "buffer has zero size");
    (void)rwFlag; (void)userData;

    // No password is currently supplied
    buffer[0] = '\0';

    return 0;
}

void
tlsCertKeyLoad(SSL_CTX *const context, const String *const certFile, const String *const keyFile)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM_P(VOID, context);
        FUNCTION_TEST_PARAM(STRING, certFile);
        FUNCTION_TEST_PARAM(STRING, keyFile);
    FUNCTION_TEST_END();

    ASSERT(context != NULL);
    ASSERT((certFile == NULL && keyFile == NULL) || (certFile != NULL && keyFile != NULL));

    if (certFile != NULL)
    {
        userInit();

        MEM_CONTEXT_TEMP_BEGIN()
        {
            // Set cert password callback
            SSL_CTX_set_default_passwd_cb(context, tlsCertPwd);

            // Load certificate
            cryptoError(
                SSL_CTX_use_certificate_chain_file(context, strZ(certFile)) != 1,
                strZ(strNewFmt("unable to load cert file '%s'", strZ(certFile))));

            // Check that key has the correct permissions
            const StorageInfo keyInfo = storageInfoP(
                storagePosixNewP(FSLASH_STR), keyFile, .ignoreMissing = true, .followLink = true);

            if (keyInfo.exists)
            {
                if (keyInfo.userId != userId() && keyInfo.userId != 0)                                              // {vm_covered}
                {
                    THROW_FMT(                                                                                      // {vm_covered}
                        FileReadError, "key file '%s' must be owned by the '%s' user or root", strZ(keyFile), strZ(userName()));
                }

                if ((keyInfo.userId == userId() && keyInfo.mode & (S_IRWXG | S_IRWXO)) ||                           // {vm_covered}
                    (keyInfo.userId == 0 && keyInfo.mode & (S_IWGRP | S_IXGRP | S_IRWXO)))                          // {vm_covered}
                {
                    THROW_FMT(
                        FileReadError,
                        "key file '%s' has group or other permissions\n"
                        "HINT: file must have permissions u=rw (0600) or less if owned by the '%s' user\n"
                        "HINT: file must have permissions u=rw, g=r (0640) or less if owned by root\n",
                        strZ(keyFile), strZ(userName()));
                }
            }

            // Load key and verify that the key and cert go together
            cryptoError(
                SSL_CTX_use_PrivateKey_file(context, strZ(keyFile), SSL_FILETYPE_PEM) != 1,
                strZ(strNewFmt("unable to load key file '%s'", strZ(keyFile))));

            // Verify again that the cert and key go together. It is not clear why this is needed since the key has already been
            // verified in SSL_CTX_use_PrivateKey_file(), but it may be that older versions of OpenSSL need it.
            cryptoError(
                SSL_CTX_check_private_key(context) != 1,
                strZ(strNewFmt("cert '%s' and key '%s' do not match", strZ(certFile), strZ(keyFile))));
        }
        MEM_CONTEXT_TEMP_END();
    }

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
SSL_CTX *
tlsContext(void)
{
    FUNCTION_TEST_VOID();

    cryptoInit();

    // Select the TLS method to use. To maintain compatibility with older versions of OpenSSL we need to use an SSL method, but
    // SSL versions will be excluded in SSL_CTX_set_options().
    const SSL_METHOD *const method = SSLv23_method();
    cryptoError(method == NULL, "unable to load TLS method");

    // Create TLS context
    SSL_CTX *const result = SSL_CTX_new(method);
    cryptoError(result == NULL, "unable to create TLS context");

    // Set options
    SSL_CTX_set_options(
        result,
        // Disable SSL
        SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 |
        // Disable compression
        SSL_OP_NO_COMPRESSION);

    // Disable auto-retry to prevent SSL_read() from hanging
    SSL_CTX_clear_mode(result, SSL_MODE_AUTO_RETRY);

    FUNCTION_TEST_RETURN(result);
}
