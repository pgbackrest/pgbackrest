/***********************************************************************************************************************************
Io Server Interface
***********************************************************************************************************************************/
#include "build.auto.h"

#include "common/debug.h"
#include "common/io/server.h"
#include "common/log.h"
#include "common/memContext.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct IoServer
{
    IoServerPub pub;                                                // Publicly accessible variables
};

/**********************************************************************************************************************************/
FN_EXTERN IoServer *
ioServerNew(void *const driver, const IoServerInterface *const interface)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM_P(VOID, driver);
        FUNCTION_LOG_PARAM(IO_SERVER_INTERFACE, interface);
    FUNCTION_LOG_END();

    ASSERT(driver != NULL);
    ASSERT(interface != NULL);
    ASSERT(interface->type != 0);
    ASSERT(interface->name != NULL);
    ASSERT(interface->accept != NULL);
    ASSERT(interface->toLog != NULL);

    OBJ_NEW_BEGIN(IoServer, .childQty = MEM_CONTEXT_QTY_MAX)
    {
        *this = (IoServer)
        {
            .pub =
            {
                .driver = objMoveToInterface(driver, this, memContextPrior()),
                .interface = interface,
            },
        };
    }
    OBJ_NEW_END();

    FUNCTION_LOG_RETURN(IO_SERVER, this);
}

/**********************************************************************************************************************************/
FN_EXTERN void
ioServerToLog(const IoServer *const this, StringStatic *const debugLog)
{
    strStcCat(debugLog, "{type: ");
    strStcResultSizeInc(debugLog, strIdToLog(this->pub.interface->type, strStcRemains(debugLog), strStcRemainsSize(debugLog)));

    strStcCat(debugLog, ", driver: ");
    this->pub.interface->toLog(this->pub.driver, debugLog);
    strStcCatChr(debugLog, '}');
}
