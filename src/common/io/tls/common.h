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
// Convert an ASN1 string used in certificates to a Buffer
FN_EXTERN Buffer *tlsAsn1ToBuf(const ASN1_STRING *nameAsn1);

// Convert an ASN1 string used in certificates to a String
FN_EXTERN String *tlsAsn1ToStr(const ASN1_STRING *nameAsn1);

// Get common name from a certificate
FN_EXTERN String *tlsCertCommonName(X509 *certificate);

// Reject embedded nulls in certificate common or alternative name to prevent attacks like CVE-2009-4034
FN_EXTERN void tlsCertNameVerify(const String *name);

// Load certificate and key and check that they match
FN_EXTERN void tlsCertKeyLoad(SSL_CTX *context, const String *certFile, const String *keyFile);

// Create TLS context
FN_EXTERN SSL_CTX *tlsContext(void);

#endif
