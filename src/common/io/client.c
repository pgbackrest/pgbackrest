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
IoClient *
ioClientNew(void *driver, const IoClientInterface *interface)
{
    FUNCTION_LOG_BEGIN(logLevelTrace)
        FUNCTION_LOG_PARAM_P(VOID, driver);
        FUNCTION_LOG_PARAM(IO_CLIENT_INTERFACE, interface);
    FUNCTION_LOG_END();

    ASSERT(driver != NULL);
    ASSERT(interface != NULL);
    ASSERT(interface->type != 0);
    ASSERT(interface->name != NULL);
    ASSERT(interface->open != NULL);
    ASSERT(interface->toLog != NULL);

    IoClient *this = memNew(sizeof(IoClient));

    *this = (IoClient)
    {
        .pub =
        {
            .memContext = memContextCurrent(),
            .driver = driver,
            .interface = interface,
        },
    };

    FUNCTION_LOG_RETURN(IO_CLIENT, this);
}

/**********************************************************************************************************************************/
String *
ioClientToLog(const IoClient *this)
{
    return strNewFmt(
        "{type: %s, driver: %s}", strZ(strIdToStr(this->pub.interface->type)), strZ(this->pub.interface->toLog(this->pub.driver)));
}
