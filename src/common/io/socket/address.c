/***********************************************************************************************************************************
Address Lookup
***********************************************************************************************************************************/
#include "build.auto.h"

#include "common/debug.h"
#include "common/io/socket/address.h"
#include "common/log.h"
#include "common/memContext.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct AddressInfo
{
    AddressInfoPub pub;                                             // Publicly accessible variables
};

/***********************************************************************************************************************************
Free address list
***********************************************************************************************************************************/
static void
addrInfoFreeResource(THIS_VOID)
{
    THIS(AddressInfo);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(ADDRESS_INFO, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    freeaddrinfo(*(struct addrinfo **)lstGet(this->pub.list, 0));

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
FN_EXTERN AddressInfo *
addrInfoNew(const String *const host, unsigned int port)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STRING, host);
        FUNCTION_LOG_PARAM(UINT, port);
    FUNCTION_LOG_END();

    ASSERT(host != NULL);
    ASSERT(port != 0);

    OBJ_NEW_BEGIN(AddressInfo, .childQty = MEM_CONTEXT_QTY_MAX, .callbackQty = 1)
    {
        *this = (AddressInfo)
        {
            .pub =
            {
                .host = strDup(host),
                .port = port,
                .list = lstNewP(sizeof(struct addrinfo *)),
            },
        };

        MEM_CONTEXT_TEMP_BEGIN()
        {
            // Set hints that narrow the type of address we are looking for -- we'll take ipv4 or ipv6
            struct addrinfo hints = (struct addrinfo)
            {
                .ai_family = AF_UNSPEC,
                .ai_flags = AI_PASSIVE,
                .ai_socktype = SOCK_STREAM,
                .ai_protocol = IPPROTO_TCP,
            };

            // Convert the port to a zero-terminated string for use with getaddrinfo()
            char portZ[CVT_BASE10_BUFFER_SIZE];
            cvtUIntToZ(port, portZ, sizeof(portZ));

            // Do the lookup
            struct addrinfo *result;
            int error;

            if ((error = getaddrinfo(strZ(host), portZ, &hints, &result)) != 0)
                THROW_FMT(HostConnectError, "unable to get address for '%s': [%d] %s", strZ(host), error, gai_strerror(error));

            // Set free callback to ensure address info is freed
            memContextCallbackSet(objMemContext(this), addrInfoFreeResource, this);

            // Convert address linked list to list
            lstAdd(this->pub.list, &result);

            while (result->ai_next != NULL)
            {
                lstAdd(this->pub.list, &result->ai_next);
                result = result->ai_next;
            }
        }
        MEM_CONTEXT_TEMP_END();
    }
    OBJ_NEW_END();

    FUNCTION_LOG_RETURN(ADDRESS_INFO, this);
}

/**********************************************************************************************************************************/
FN_EXTERN String *
addrInfoToStr(const struct addrinfo *const addrInfo)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM_P(VOID, addrInfo);
    FUNCTION_TEST_END();

    char buffer[48];

    if (getnameinfo(addrInfo->ai_addr, addrInfo->ai_addrlen, buffer, sizeof(buffer), 0, 0, NI_NUMERICHOST) != 0)
        FUNCTION_TEST_RETURN(STRING, strNewZ("invalid"));

    FUNCTION_TEST_RETURN(STRING, strNewZ(buffer));
}

/**********************************************************************************************************************************/
FN_EXTERN void
addrInfoToLog(const AddressInfo *const this, StringStatic *const debugLog)
{
    char buffer[48];

    strStcFmt(debugLog, "{list: [");

    for (unsigned int listIdx = 0; listIdx < addrInfoSize(this); listIdx++)
    {
        const struct addrinfo *const addrInfo = addrInfoGet(this, listIdx);

        if (listIdx != 0)
            strStcCat(debugLog, ", ");

        if (getnameinfo(addrInfo->ai_addr, addrInfo->ai_addrlen, buffer, sizeof(buffer), 0, 0, NI_NUMERICHOST) != 0)
            strStcCat(debugLog, "invalid");
        else
            strStcCat(debugLog, buffer);
    }

    strStcCat(debugLog, "]}");
}
