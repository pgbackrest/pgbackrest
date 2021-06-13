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
IoServer *
ioServerNew(void *const driver, const IoServerInterface *const interface)
{
    FUNCTION_LOG_BEGIN(logLevelTrace)
        FUNCTION_LOG_PARAM_P(VOID, driver);
        FUNCTION_LOG_PARAM(IO_SERVER_INTERFACE, interface);
    FUNCTION_LOG_END();

    ASSERT(driver != NULL);
    ASSERT(interface != NULL);
    ASSERT(interface->type != 0);
    ASSERT(interface->name != NULL);
    ASSERT(interface->accept != NULL);
    ASSERT(interface->toLog != NULL);

    IoServer *this = memNew(sizeof(IoServer));

    *this = (IoServer)
    {
        .pub =
        {
            .memContext = memContextCurrent(),
            .driver = driver,
            .interface = interface,
        },
    };

    FUNCTION_LOG_RETURN(IO_SERVER, this);
}

/**********************************************************************************************************************************/
String *
ioServerToLog(const IoServer *const this)
{
    return strNewFmt(
        "{type: %s, driver: %s}", strZ(strIdToStr(this->pub.interface->type)), strZ(this->pub.interface->toLog(this->pub.driver)));
}
