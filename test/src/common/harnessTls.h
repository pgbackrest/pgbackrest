/***********************************************************************************************************************************
Tls Test Harness

Simple TLS server for testing TLS client functionality.
***********************************************************************************************************************************/
#ifndef TEST_COMMON_HARNESS_TLS_H
#define TEST_COMMON_HARNESS_TLS_H

/***********************************************************************************************************************************
Path and prefix for test certificates
***********************************************************************************************************************************/
#define TEST_CERTIFICATE_PREFIX                                     "test/certificate/pgbackrest-test"

/***********************************************************************************************************************************
Tls test defaults
***********************************************************************************************************************************/
#define TLS_CERT_FAKE_PATH                                          "/etc/fake-cert"
#define TLS_CERT_TEST_CERT                                          TLS_CERT_FAKE_PATH "/pgbackrest-test.crt"
#define TLS_CERT_TEST_KEY                                           TLS_CERT_FAKE_PATH "/pgbackrest-test.key"

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
void harnessTlsServerInit(unsigned int port, const char *serverCert, const char *serverKey);

// Initialize TLS with default parameters
void harnessTlsServerInitDefault(void);

void harnessTlsServerAccept(void);
void harnessTlsServerExpect(const char *expected);
void harnessTlsServerReply(const char *reply);
void harnessTlsServerClose(void);

/***********************************************************************************************************************************
Getters/Setters
***********************************************************************************************************************************/
// Hostname to use for testing -- this will vary based on whether the test is running in a container
const String *harnessTlsTestHost(void);

// Port to use for testing.  This will be unique for each test running in parallel to avoid conflicts
unsigned int harnessTlsTestPort(void);

#endif
