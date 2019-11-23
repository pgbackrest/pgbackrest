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

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
void protocolKeepAlive(void);
ProtocolClient *protocolLocalGet(ProtocolStorageType protocolStorageType, unsigned int hostId, unsigned int protocolId);
ProtocolClient *protocolRemoteGet(ProtocolStorageType protocolStorageType, unsigned int hostId);

/***********************************************************************************************************************************
Getters
***********************************************************************************************************************************/
bool pgIsLocal(unsigned int hostId);
bool repoIsLocal(void);
void repoIsLocalVerify(void);

// Get enum/string for protocol storage type
ProtocolStorageType protocolStorageTypeEnum(const String *type);
const String *protocolStorageTypeStr(ProtocolStorageType type);

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
void protocolFree(void);

#endif
