/***********************************************************************************************************************************
Io Client Interface
***********************************************************************************************************************************/
#include "build.auto.h"

#include "common/debug.h"
#include "common/io/client.h"
#include "common/log.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct IoClient
{
    IoClientPub pub;                                                // Publicly accessible variables
};

/**********************************************************************************************************************************/
FN_EXTERN IoClient *
ioClientNew(void *const driver, const IoClientInterface *const interface)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM_P(VOID, driver);
        FUNCTION_LOG_PARAM(IO_CLIENT_INTERFACE, interface);
    FUNCTION_LOG_END();

    ASSERT(driver != NULL);
    ASSERT(interface != NULL);
    ASSERT(interface->type != 0);
    ASSERT(interface->name != NULL);
    ASSERT(interface->open != NULL);
    ASSERT(interface->toLog != NULL);

    OBJ_NEW_BEGIN(IoClient, .childQty = MEM_CONTEXT_QTY_MAX)
    {
        *this = (IoClient)
        {
            .pub =
            {
                .driver = objMoveToInterface(driver, this, memContextPrior()),
                .interface = interface,
            },
        };
    }
    OBJ_NEW_END();

    FUNCTION_LOG_RETURN(IO_CLIENT, this);
}

/**********************************************************************************************************************************/
FN_EXTERN void
ioClientToLog(const IoClient *const this, StringStatic *const debugLog)
{
    strStcCat(debugLog, "{type: ");
    strStcResultSizeInc(debugLog, strIdToLog(this->pub.interface->type, strStcRemains(debugLog), strStcRemainsSize(debugLog)));

    strStcCat(debugLog, ", driver: ");
    this->pub.interface->toLog(this->pub.driver, debugLog);
    strStcCatChr(debugLog, '}');
}
