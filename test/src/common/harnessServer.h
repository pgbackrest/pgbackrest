/***********************************************************************************************************************************
Server Test Harness

Simple server for testing client functionality using multiple protocols and connections. Server behavior can be scripted from the
client using the hrnServerScript() functions.
***********************************************************************************************************************************/
#ifndef TEST_COMMON_HARNESS_SERVER_H
#define TEST_COMMON_HARNESS_SERVER_H

#include "common/io/read.h"
#include "common/io/write.h"

/***********************************************************************************************************************************
Server protocol type
***********************************************************************************************************************************/
typedef enum
{
    hrnServerProtocolSocket,
    hrnServerProtocolTls,
} HrnServerProtocol;

/***********************************************************************************************************************************
Maximum number of ports allowed for each test
***********************************************************************************************************************************/
#define HRN_SERVER_PORT_MAX                                         4

/***********************************************************************************************************************************
Path and prefix for test certificates
***********************************************************************************************************************************/
#define HRN_SERVER_CERT_PREFIX                                     "test/certificate/pgbackrest-test-"
#define HRN_SERVER_CERT                                            HRN_PATH_REPO "/" HRN_SERVER_CERT_PREFIX "server.crt"
#define HRN_SERVER_KEY                                             HRN_PATH_REPO "/" HRN_SERVER_CERT_PREFIX "server.key"
#define HRN_SERVER_CA                                              HRN_PATH_REPO "/" HRN_SERVER_CERT_PREFIX "ca.crt"

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Run server
typedef struct HrnServerRunParam
{
    VAR_PARAM_HEADER;
    unsigned int port;                                              // Server port, defaults to hrnServerPort(0)
    const String *certificate;                                      // TLS certificate when protocol = hrnServerProtocolTls
    const String *key;                                              // TLS key when protocol = hrnServerProtocolTls
} HrnServerRunParam;

#define hrnServerRunP(read, protocol, ...)                                                                                         \
    hrnServerRun(read, protocol, (HrnServerRunParam){VAR_PARAM_INIT, __VA_ARGS__})

void hrnServerRun(IoRead *read, HrnServerProtocol protocol, HrnServerRunParam param);

// Begin/end server script
IoWrite *hrnServerScriptBegin(IoWrite *write);
void hrnServerScriptEnd(IoWrite *write);

// Abort the server session (i.e. don't perform proper TLS shutdown)
void hrnServerScriptAbort(IoWrite *write);

// Accept new connection
void hrnServerScriptAccept(IoWrite *write);

// Close the connection
void hrnServerScriptClose(IoWrite *write);

// Expect the specfified string
void hrnServerScriptExpect(IoWrite *write, const String *data);
void hrnServerScriptExpectZ(IoWrite *write, const char *data);

// Reply with the specfified string
void hrnServerScriptReply(IoWrite *write, const String *data);
void hrnServerScriptReplyZ(IoWrite *write, const char *data);

// Sleep specfified milliseconds
void hrnServerScriptSleep(IoWrite *write, TimeMSec sleepMs);

/***********************************************************************************************************************************
Getters/Setters
***********************************************************************************************************************************/
// Hostname to use for testing -- this will vary based on whether the test is running in a container
const String *hrnServerHost(void);

// Port to use for testing. This will be unique for each test running in parallel to avoid conflicts. A range is allocated to each
// test so multiple ports can be requested.
unsigned int hrnServerPort(unsigned int portIdx);

#endif
