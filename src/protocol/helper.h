/***********************************************************************************************************************************
Protocol Helper
***********************************************************************************************************************************/
#ifndef PROTOCOL_HELPER_H
#define PROTOCOL_HELPER_H

#include "common/type/stringId.h"

/***********************************************************************************************************************************
Protocol storage type enum
***********************************************************************************************************************************/
typedef enum
{
    protocolStorageTypePg = STRID5("pg", 0xf00),
    protocolStorageTypeRepo = STRID5("repo", 0x7c0b20),
} ProtocolStorageType;

#include "common/io/server.h"
#include "protocol/server.h"

/***********************************************************************************************************************************
Constants
***********************************************************************************************************************************/
#define PROTOCOL_SERVICE_LOCAL                                      "local"
    STRING_DECLARE(PROTOCOL_SERVICE_LOCAL_STR);
#define PROTOCOL_SERVICE_REMOTE                                     "remote"
    STRING_DECLARE(PROTOCOL_SERVICE_REMOTE_STR);

/***********************************************************************************************************************************
Getters/Setters
***********************************************************************************************************************************/
// Is pg local?
FV_EXTERN bool pgIsLocal(unsigned int pgIdx);

// Error if PostgreSQL is not local, i.e. pg-host is set
FV_EXTERN void pgIsLocalVerify(void);

// Is the repository local?
FV_EXTERN bool repoIsLocal(unsigned int repoIdx);

// Error if the repository is not local
FV_EXTERN void repoIsLocalVerify(void);
FV_EXTERN void repoIsLocalVerifyIdx(unsigned int repoIdx);

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Send keepalives to all remotes
FV_EXTERN void protocolKeepAlive(void);

// Local protocol client
FV_EXTERN ProtocolClient *protocolLocalGet(ProtocolStorageType protocolStorageType, unsigned int hostId, unsigned int protocolId);

// Free (shutdown) a local
FV_EXTERN void protocolLocalFree(unsigned int protocolId);

// Remote protocol client
FV_EXTERN ProtocolClient *protocolRemoteGet(ProtocolStorageType protocolStorageType, unsigned int hostId);

// Free (shutdown) a remote
FV_EXTERN void protocolRemoteFree(unsigned int hostId);

// Initialize a server
FV_EXTERN ProtocolServer *protocolServer(IoServer *const tlsServer, IoSession *const socketSession);

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
FV_EXTERN void protocolFree(void);

#endif
