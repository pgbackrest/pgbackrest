/***********************************************************************************************************************************
TLS Common
***********************************************************************************************************************************/
#ifndef COMMON_IO_TLS_COMMON_H
#define COMMON_IO_TLS_COMMON_H

#include <openssl/x509v3.h>

#include "common/type/string.h"

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Get common name from a certificate
String *tlsCertificateCommonName(X509 *certificate);

// Reject embedded nulls in certificate common or alternative name to prevent attacks like CVE-2009-4034
void tlsCertificateNameVerify(const String *name);

// Convert an ASN1 string used in certificates to a String
String *tlsAsn1ToStr(ASN1_STRING *nameAsn1);

#endif
