/***********************************************************************************************************************************
Io Client Interface
***********************************************************************************************************************************/
#include "build.auto.h"

#include "common/debug.h"
#include "common/io/client.intern.h"
#include "common/log.h"
#include "common/memContext.h"
#include "common/type/object.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct IoClient
{
    MemContext *memContext;                                         // Mem context
    void *driver;                                                   // Driver object
    const IoClientInterface *interface;                             // Driver interface
} IoClient;

OBJECT_DEFINE_FREE(IO_CLIENT);

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

    IoClient *this = memNew(sizeof(IoClient));

    *this = (IoClient)
    {
        .memContext = memContextCurrent(),
        .driver = driver,
        .interface = interface,
    };

    FUNCTION_LOG_RETURN(IO_CLIENT, this);
}

/**********************************************************************************************************************************/
IoSession *
ioClientOpen(IoClient *this)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(IO_CLIENT, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    FUNCTION_LOG_RETURN(IO_SESSION, this->interface->open(this->driver));
}
