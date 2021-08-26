/***********************************************************************************************************************************
TLS Common
***********************************************************************************************************************************/
#include "build.auto.h"

#include "common/debug.h"
#include "common/io/tls/common.h"

/**********************************************************************************************************************************/
String *
tlsAsn1ToStr(const ASN1_STRING *const nameAsn1)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM_P(VOID, nameAsn1);
    FUNCTION_TEST_END();

    // The name should not be null
    if (nameAsn1 == NULL)                                                                                           // {vm_covered}
        THROW(CryptoError, "TLS certificate name entry is missing");

    FUNCTION_TEST_RETURN(                                                                                           // {vm_covered}
        strNewN(
#if OPENSSL_VERSION_NUMBER < 0x10100000L
            (const char *)ASN1_STRING_data(nameAsn1),
#else
            (const char *)ASN1_STRING_get0_data(nameAsn1),
#endif
            (size_t)ASN1_STRING_length(nameAsn1)));
}

/**********************************************************************************************************************************/
void
tlsCertificateNameVerify(const String *const name)
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
tlsCertificateCommonName(const X509 *const certificate)                                                             // {vm_covered}
{
    FUNCTION_TEST_BEGIN();                                                                                          // {vm_covered}
        FUNCTION_TEST_PARAM_P(VOID, certificate);                                                                   // {vm_covered}
    FUNCTION_TEST_END();                                                                                            // {vm_covered}

    X509_NAME *const subjectName = X509_get_subject_name(certificate);                                              // {vm_covered}
    CHECK(subjectName != NULL);                                                                                     // {vm_covered}

    const int commonNameIndex = X509_NAME_get_index_by_NID(subjectName, NID_commonName, -1);                        // {vm_covered}
    CHECK(commonNameIndex >= 0);                                                                                    // {vm_covered}

    String *result = tlsAsn1ToStr(X509_NAME_ENTRY_get_data(X509_NAME_get_entry(subjectName, commonNameIndex)));     // {vm_covered}

    // Check for NULLs in the name
    tlsCertificateNameVerify(result);                                                                               // {vm_covered}

    FUNCTION_TEST_RETURN(result);                                                                                   // {vm_covered}
}
