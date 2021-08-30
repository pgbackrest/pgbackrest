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
// Convert an ASN1 string used in certificates to a String
String *tlsAsn1ToStr(ASN1_STRING *nameAsn1);

// Get common name from a certificate
String *tlsCertCommonName(X509 *certificate);

// Reject embedded nulls in certificate common or alternative name to prevent attacks like CVE-2009-4034
void tlsCertNameVerify(const String *name);

// Load certificate and key and check that they match
void tlsCertKeyLoad(SSL_CTX *context, const String *certFile, const String *keyFile);

// Create TLS context
SSL_CTX *tlsContext(void);

// Load certificate revocation list
void tlsCrlLoad(SSL_CTX *context, const String *crlFile);

#endif
