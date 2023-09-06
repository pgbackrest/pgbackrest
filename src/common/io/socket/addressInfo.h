/***********************************************************************************************************************************
Address Info

!!! Get address info for a host/address
***********************************************************************************************************************************/
#ifndef COMMON_IO_SOCKET_ADDRESSINFO_H
#define COMMON_IO_SOCKET_ADDRESSINFO_H

#include <netdb.h>

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct AddressInfo AddressInfo;

#include "common/type/object.h"

/***********************************************************************************************************************************
Constructor
***********************************************************************************************************************************/
FN_EXTERN AddressInfo *addrInfoNew(const String *const host, unsigned int port);

/***********************************************************************************************************************************
Getters/Setters
***********************************************************************************************************************************/
typedef struct AddressInfoPub
{
    List *list;                                                     // List of addresses for the host
} AddressInfoPub;

// Size of address list
FN_INLINE_ALWAYS unsigned int
addrInfoSize(const AddressInfo *const this)
{
    return lstSize(THIS_PUB(AddressInfo)->list);
}

// Get address
FN_INLINE_ALWAYS const struct addrinfo *
addrInfoGet(const AddressInfo *const this, unsigned int index)
{
    return *(const struct addrinfo **)lstGet(THIS_PUB(AddressInfo)->list, index);
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
Macros for function logging
***********************************************************************************************************************************/
// !!! Make this a real log function
#define FUNCTION_LOG_ADDRESS_INFO_TYPE                                                                                             \
    AddressInfo *
#define FUNCTION_LOG_ADDRESS_INFO_FORMAT(value, buffer, bufferSize)                                                                \
    objNameToLog(value, "AddressInfo", buffer, bufferSize)

#endif
