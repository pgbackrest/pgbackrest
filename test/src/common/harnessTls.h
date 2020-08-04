/***********************************************************************************************************************************
TLS Test Harness

Simple TLS server for testing TLS client functionality.
***********************************************************************************************************************************/
#ifndef TEST_COMMON_HARNESS_TLS_H
#define TEST_COMMON_HARNESS_TLS_H

#include "common/debug.h"
#include "common/io/tls/session.h"

/***********************************************************************************************************************************
Path and prefix for test certificates
***********************************************************************************************************************************/
#define TEST_CERTIFICATE_PREFIX                                     "test/certificate/pgbackrest-test"

/***********************************************************************************************************************************
TLS test defaults
***********************************************************************************************************************************/
#define TLS_CERT_FAKE_PATH                                          "/etc/fake-cert"
#define TLS_CERT_TEST_CERT                                          TLS_CERT_FAKE_PATH "/pgbackrest-test.crt"

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Begin/end client
void hrnTlsClientBegin(IoWrite *write);
void hrnTlsClientEnd(void);

// Run server
void hrnTlsServerRun(IoRead *read);

// Abort the server session (i.e. don't perform proper TLS shutdown)
void hrnTlsServerAbort(void);

// Accept new TLS connection
void hrnTlsServerAccept(void);

// Close the TLS connection
void hrnTlsServerClose(void);

// Expect the specfified string
void hrnTlsServerExpect(const String *data);
void hrnTlsServerExpectZ(const char *data);

// Reply with the specfified string
void hrnTlsServerReply(const String *data);
void hrnTlsServerReplyZ(const char *data);

// Sleep specfified milliseconds
void hrnTlsServerSleep(TimeMSec sleepMs);

/***********************************************************************************************************************************
Getters/Setters
***********************************************************************************************************************************/
// Hostname to use for testing -- this will vary based on whether the test is running in a container
const String *hrnTlsServerHost(void);

// Port to use for testing. This will be unique for each test running in parallel to avoid conflicts
unsigned int hrnTlsServerPort(void);

#endif
