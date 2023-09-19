/***********************************************************************************************************************************
Address Info

Get address info for a host/address. If the input is a host then there may be multiple addresses that map to the host.
***********************************************************************************************************************************/
#ifndef COMMON_IO_SOCKET_ADDRESSINFO_H
#define COMMON_IO_SOCKET_ADDRESSINFO_H

#include <netdb.h>

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct AddressInfo AddressInfo;

#include "common/type/list.h"
#include "common/type/object.h"
#include "common/type/string.h"

/***********************************************************************************************************************************
Constructor
***********************************************************************************************************************************/
FN_EXTERN AddressInfo *addrInfoNew(const String *const host, unsigned int port);

/***********************************************************************************************************************************
Getters/Setters
***********************************************************************************************************************************/
typedef struct AddressInfoPub
{
    const String *host;                                             // Host for address info lookup
    unsigned int port;                                              // Port for address info lookup
    List *list;                                                     // List of addresses for the host
} AddressInfoPub;

// Get address
FN_INLINE_ALWAYS const struct addrinfo *
addrInfoGet(const AddressInfo *const this, const unsigned int index)
{
    return *(const struct addrinfo **)lstGet(THIS_PUB(AddressInfo)->list, index);
}

// Get lookup host
FN_INLINE_ALWAYS const String *
addrInfoHost(const AddressInfo *const this)
{
    return THIS_PUB(AddressInfo)->host;
}

// Get lookup port
FN_INLINE_ALWAYS unsigned int
addrInfoPort(const AddressInfo *const this)
{
    return THIS_PUB(AddressInfo)->port;
}

// Size of address list
FN_INLINE_ALWAYS unsigned int
addrInfoSize(const AddressInfo *const this)
{
    return lstSize(THIS_PUB(AddressInfo)->list);
}

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
FN_INLINE_ALWAYS void
addrInfoFree(AddressInfo *const this)
{
    objFree(this);
}

/***********************************************************************************************************************************
Helper functions
***********************************************************************************************************************************/
// Convert address info to string
FN_EXTERN String *addrInfoToStr(const struct addrinfo *addrInfo);

// Generate name for the host/port/address combination (address is omitted if it equals host)
FN_EXTERN String *addrInfoToName(const String *host, unsigned int port, const struct addrinfo *addrInfo);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
FN_EXTERN void addrInfoToLog(const AddressInfo *this, StringStatic *debugLog);

#define FUNCTION_LOG_ADDRESS_INFO_TYPE                                                                                             \
    AddressInfo *
#define FUNCTION_LOG_ADDRESS_INFO_FORMAT(value, buffer, bufferSize)                                                                \
    FUNCTION_LOG_OBJECT_FORMAT(value, addrInfoToLog, buffer, bufferSize)

#endif
