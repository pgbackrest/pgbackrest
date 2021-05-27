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

#include "protocol/client.h"

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
bool pgIsLocal(unsigned int pgIdx);

// Error if PostgreSQL is not local, i.e. pg-host is set
void pgIsLocalVerify(void);

// Is the repository local?
bool repoIsLocal(unsigned int repoIdx);

// Error if the repository is not local
void repoIsLocalVerify(void);
void repoIsLocalVerifyIdx(unsigned int repoIdx);

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Send keepalives to all remotes
void protocolKeepAlive(void);

// Local protocol client
ProtocolClient *protocolLocalGet(ProtocolStorageType protocolStorageType, unsigned int hostId, unsigned int protocolId);

// Free (shutdown) a local
void protocolLocalFree(unsigned int protocolId);

// Remote protocol client
ProtocolClient *protocolRemoteGet(ProtocolStorageType protocolStorageType, unsigned int hostId);

// Free (shutdown) a remote
void protocolRemoteFree(unsigned int hostId);

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
void protocolFree(void);

#endif
