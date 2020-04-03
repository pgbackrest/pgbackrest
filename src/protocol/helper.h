/***********************************************************************************************************************************
Protocol Helper
***********************************************************************************************************************************/
#ifndef PROTOCOL_HELPER_H
#define PROTOCOL_HELPER_H

/***********************************************************************************************************************************
Protocol storage type enum
***********************************************************************************************************************************/
typedef enum
{
    protocolStorageTypeRepo,
    protocolStorageTypePg,
} ProtocolStorageType;

#include "protocol/client.h"

/***********************************************************************************************************************************
Constants
***********************************************************************************************************************************/
#define PROTOCOL_SERVICE_LOCAL                                      "local"
    STRING_DECLARE(PROTOCOL_SERVICE_LOCAL_STR);
#define PROTOCOL_SERVICE_REMOTE                                     "remote"
    STRING_DECLARE(PROTOCOL_SERVICE_REMOTE_STR);

#define PROTOCOL_REMOTE_TYPE_PG                                     "pg"
#define PROTOCOL_REMOTE_TYPE_REPO                                   "repo"

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Send keepalives to all remotes
void protocolKeepAlive(void);

// Local protocol client
ProtocolClient *protocolLocalGet(ProtocolStorageType protocolStorageType, unsigned int hostId, unsigned int protocolId);

// Remote protocol client
ProtocolClient *protocolRemoteGet(ProtocolStorageType protocolStorageType, unsigned int hostId);

// Free (shutdown) a remote
void protocolRemoteFree(unsigned int hostId);

/***********************************************************************************************************************************
Getters
***********************************************************************************************************************************/
// Is pg local?
bool pgIsLocal(unsigned int hostId);

// Error if PostgreSQL is not local, i.e. pg-host is set
void pgIsLocalVerify(void);

// Is the repository local?
bool repoIsLocal(void);

// Error if the repository is not local
void repoIsLocalVerify(void);

// Get enum/string for protocol storage type
ProtocolStorageType protocolStorageTypeEnum(const String *type);
const String *protocolStorageTypeStr(ProtocolStorageType type);

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
void protocolFree(void);

#endif
