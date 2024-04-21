/***********************************************************************************************************************************
Address Info
***********************************************************************************************************************************/
#include "build.auto.h"

#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>

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
    struct addrinfo *info;                                          // Linked list of addresses
};

/***********************************************************************************************************************************
Local variables
***********************************************************************************************************************************/
// Store preferred addresses for hosts
typedef struct AddressInfoPreference
{
    const String *const host;                                       // Host
    String *address;                                                // Preferred address for host
} AddressInfoPreference;

static struct AddressInfoLocal
{
    MemContext *memContext;                                         // Mem context
    List *prefList;                                                 // List of preferred addresses for hosts
} addressInfoLocal;

/***********************************************************************************************************************************
Free addrinfo linked list allocated by getaddrinfo()
***********************************************************************************************************************************/
static void
addrInfoFreeResource(THIS_VOID)
{
    THIS(AddressInfo);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(ADDRESS_INFO, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    freeaddrinfo(this->info);

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
FN_EXTERN void
addrInfoSort(AddressInfo *const this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ADDRESS_INFO, this);
    FUNCTION_TEST_END();

    // Only sort if there is more than one address
    if (lstSize(this->pub.list) > 1)
    {
        // By default start with IPv6 and first address
        int family = AF_INET6;
        unsigned int addrIdx = 0;

        // If a preferred address is in the list then move it to the first position and update family
        const AddressInfoPreference *const addrPref = lstFind(addressInfoLocal.prefList, &this->pub.host);

        if (addrPref != NULL)
        {
            AddressInfoItem *const addrFindItem = lstFind(this->pub.list, &addrPref->address);

            if (addrFindItem != NULL)
            {
                // Swap with first address if the address is not already first
                AddressInfoItem *const addrFirstItem = lstGet(this->pub.list, 0);

                if (addrFirstItem != addrFindItem)
                {
                    const AddressInfoItem addrCopyItem = *addrFirstItem;
                    *addrFirstItem = *addrFindItem;
                    *addrFindItem = addrCopyItem;
                }

                // Set family and skip first address
                family = addrFirstItem->info->ai_family == AF_INET6 ? AF_INET : AF_INET6;
                addrIdx++;
            }
        }

        // Alternate IPv6 and IPv4 addresses
        for (; addrIdx < lstSize(this->pub.list); addrIdx++)
        {
            AddressInfoItem *const addrItem = lstGet(this->pub.list, addrIdx);

            // If not the family we expect, search for one
            if (addrItem->info->ai_family != family)
            {
                unsigned int addrFindIdx = addrIdx + 1;

                for (; addrFindIdx < lstSize(this->pub.list); addrFindIdx++)
                {
                    AddressInfoItem *const addrFindItem = lstGet(this->pub.list, addrFindIdx);

                    if (addrFindItem->info->ai_family == family)
                    {
                        // Swap addresses
                        const AddressInfoItem addrCopyItem = *addrItem;
                        *addrItem = *addrFindItem;
                        *addrFindItem = addrCopyItem;

                        // Move on to next address
                        break;
                    }
                }

                // Address of the required family not found so sorting is done
                if (addrFindIdx == lstSize(this->pub.list))
                    break;
            }

            // Switch family
            family = family == AF_INET6 ? AF_INET : AF_INET6;
        }
    }

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
FN_EXTERN AddressInfo *
addrInfoNew(const String *const host, const unsigned int port)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STRING, host);
        FUNCTION_LOG_PARAM(UINT, port);
    FUNCTION_LOG_END();

    ASSERT(host != NULL);
    ASSERT(port != 0);

    // Initialize mem context and preference list
    if (addressInfoLocal.memContext == NULL)
    {
        MEM_CONTEXT_BEGIN(memContextTop())
        {
            MEM_CONTEXT_NEW_BEGIN(AddressInfo, .childQty = MEM_CONTEXT_QTY_MAX)
            {
                addressInfoLocal.memContext = MEM_CONTEXT_NEW();
                addressInfoLocal.prefList = lstNewP(sizeof(AddressInfoPreference), .comparator = lstComparatorStr);
            }
            MEM_CONTEXT_NEW_END();
        }
        MEM_CONTEXT_END();
    }

    OBJ_NEW_BEGIN(AddressInfo, .childQty = MEM_CONTEXT_QTY_MAX, .callbackQty = 1)
    {
        *this = (AddressInfo)
        {
            .pub =
            {
                .host = strDup(host),
                .port = port,
                .list = lstNewP(sizeof(AddressInfoItem), .comparator = lstComparatorStr),
            },
        };

        MEM_CONTEXT_TEMP_BEGIN()
        {
            // Set hints that narrow the type of address we are looking for -- we'll take ipv4 or ipv6
            struct addrinfo hints = (struct addrinfo)
            {
                .ai_family = AF_UNSPEC,
                .ai_flags = AI_PASSIVE | AI_NUMERICSERV,
                .ai_socktype = SOCK_STREAM,
                .ai_protocol = IPPROTO_TCP,
            };

            // Convert the port to a zero-terminated string for use with getaddrinfo()
            char portZ[CVT_BASE10_BUFFER_SIZE];
            cvtUIntToZ(port, portZ, sizeof(portZ));

            // Do the lookup
            int error;

            if ((error = getaddrinfo(strZ(host), portZ, &hints, &this->info)) != 0)
                THROW_FMT(HostConnectError, "unable to get address for '%s': [%d] %s", strZ(host), error, gai_strerror(error));

            // Convert address linked list to list
            MEM_CONTEXT_OBJ_BEGIN(this->pub.list)
            {
                struct addrinfo *info = this->info;

                lstAdd(this->pub.list, &(AddressInfoItem){.name = addrInfoToStr(info), .info = info});

                // Set free callback to ensure address info is freed
                memContextCallbackSet(objMemContext(this), addrInfoFreeResource, this);

                while (info->ai_next != NULL)
                {
                    info = info->ai_next;
                    lstAdd(this->pub.list, &(AddressInfoItem){.name = addrInfoToStr(info), .info = info});
                }
            }
            MEM_CONTEXT_OBJ_END();
        }
        MEM_CONTEXT_TEMP_END();
    }
    OBJ_NEW_END();

    FUNCTION_LOG_RETURN(ADDRESS_INFO, this);
}

/**********************************************************************************************************************************/
FN_EXTERN void
addrInfoPrefer(AddressInfo *const this, const unsigned int index)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(ADDRESS_INFO, this);
        FUNCTION_LOG_PARAM(UINT, index);
    FUNCTION_LOG_END();

    AddressInfoPreference *const addrPref = lstFind(addressInfoLocal.prefList, &this->pub.host);
    const String *const address = addrInfoGet(this, index)->name;

    MEM_CONTEXT_OBJ_BEGIN(addressInfoLocal.prefList)
    {
        // If no preference set yet
        if (addrPref == NULL)
        {
            lstAdd(addressInfoLocal.prefList, &(AddressInfoPreference){.host = strDup(this->pub.host), .address = strDup(address)});
            lstSort(addressInfoLocal.prefList, sortOrderAsc);
        }
        // Else update address
        else
        {
            // Free the old address
            strFree(addrPref->address);

            // Assign new address
            addrPref->address = strDup(address);
        }
    }
    MEM_CONTEXT_OBJ_END();

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Convert address to a zero-terminated string
***********************************************************************************************************************************/
#define ADDR_INFO_STR_BUFFER_SIZE                                   48

static void
addrInfoToZ(const struct addrinfo *const addrInfo, char *const address, const size_t addressSize)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM_P(VOID, addrInfo);
        FUNCTION_TEST_PARAM_P(CHAR, address);
        FUNCTION_TEST_PARAM(SIZE, addressSize);
    FUNCTION_TEST_END();

    if (getnameinfo(addrInfo->ai_addr, addrInfo->ai_addrlen, address, (socklen_t)addressSize, 0, 0, NI_NUMERICHOST) != 0)
    {
        strncpy(address, "invalid", addressSize);
        address[addressSize - 1] = '\0';
    }

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
FN_EXTERN String *
addrInfoToName(const String *const host, const unsigned int port, const struct addrinfo *const addrInfo)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, host);
        FUNCTION_TEST_PARAM(UINT, port);
        FUNCTION_TEST_PARAM_P(VOID, addrInfo);
    FUNCTION_TEST_END();

    String *const result = strCatFmt(strNew(), "%s:%u", strZ(host), port);
    String *const address = addrInfoToStr(addrInfo);

    if (!strEq(host, address))
        strCatFmt(result, " (%s)", strZ(address));

    strFree(address);

    FUNCTION_TEST_RETURN(STRING, result);
}

/**********************************************************************************************************************************/
FN_EXTERN String *
addrInfoToStr(const struct addrinfo *const addrInfo)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM_P(VOID, addrInfo);
    FUNCTION_TEST_END();

    char address[ADDR_INFO_STR_BUFFER_SIZE];
    addrInfoToZ(addrInfo, address, sizeof(address));

    FUNCTION_TEST_RETURN(STRING, strNewZ(address));
}

/**********************************************************************************************************************************/
FN_EXTERN void
addrInfoToLog(const AddressInfo *const this, StringStatic *const debugLog)
{
    strStcFmt(debugLog, "{host: ");
    strToLog(addrInfoHost(this), debugLog);
    strStcFmt(debugLog, ", port: %u, list: [", addrInfoPort(this));

    for (unsigned int listIdx = 0; listIdx < addrInfoSize(this); listIdx++)
    {
        const AddressInfoItem *const addrItem = addrInfoGet(this, listIdx);

        if (listIdx != 0)
            strStcCat(debugLog, ", ");

        strStcCat(debugLog, strZ(addrItem->name));
    }

    strStcCat(debugLog, "]}");
}
