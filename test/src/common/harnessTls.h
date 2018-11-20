/***********************************************************************************************************************************
Tls Test Harness

Simple TLS server for testing TLS client functionality.
***********************************************************************************************************************************/
#ifndef TEST_COMMON_HARNESS_TLS_H
#define TEST_COMMON_HARNESS_TLS_H

/***********************************************************************************************************************************
Tls test defaults
***********************************************************************************************************************************/
#define TLS_TEST_HOST                                               "tls.test.pgbackrest.org"
#define TLS_TEST_PORT                                               9443

#define TLS_CERT_FAKE_PATH                                          "/etc/fake-cert"
#define TLS_CERT_TEST_CERT                                          TLS_CERT_FAKE_PATH "/pgbackrest-test.crt"
#define TLS_CERT_TEST_KEY                                           TLS_CERT_FAKE_PATH "/pgbackrest-test.key"

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
void harnessTlsServerInit(int port, const char *serverCert, const char *serverKey);

void harnessTlsServerAccept(void);
void harnessTlsServerExpect(const char *expected);
void harnessTlsServerReply(const char *reply);
void harnessTlsServerClose(void);

#endif
