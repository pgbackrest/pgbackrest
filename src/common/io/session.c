/***********************************************************************************************************************************
Io Session Interface
***********************************************************************************************************************************/
#include "build.auto.h"

#include "common/debug.h"
#include "common/io/session.intern.h"
#include "common/log.h"
#include "common/memContext.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct IoSession
{
    MemContext *memContext;                                         // Mem context
    void *driver;                                                   // Driver object
    const IoSessionInterface *interface;                            // Driver interface
};

/**********************************************************************************************************************************/
IoSession *
ioSessionNew(void *driver, const IoSessionInterface *interface)
{
    FUNCTION_LOG_BEGIN(logLevelTrace)
        FUNCTION_LOG_PARAM_P(VOID, driver);
        FUNCTION_LOG_PARAM(IO_SESSION_INTERFACE, interface);
    FUNCTION_LOG_END();

    ASSERT(driver != NULL);
    ASSERT(interface != NULL);
    ASSERT(interface->type != NULL);
    ASSERT(interface->close != NULL);
    ASSERT(interface->ioRead != NULL);
    ASSERT(interface->ioWrite != NULL);
    ASSERT(interface->role != NULL);
    ASSERT(interface->toLog != NULL);

    IoSession *this = memNew(sizeof(IoSession));

    *this = (IoSession)
    {
        .memContext = memContextCurrent(),
        .driver = driver,
        .interface = interface,
    };

    FUNCTION_LOG_RETURN(IO_SESSION, this);
}

/**********************************************************************************************************************************/
void
ioSessionClose(IoSession *this)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(IO_SESSION, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    this->interface->close(this->driver);

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
int
ioSessionFd(IoSession *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(IO_SESSION, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->interface->fd == NULL ? -1 : this->interface->fd(this->driver));
}

/**********************************************************************************************************************************/
IoRead *
ioSessionIoRead(IoSession *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(IO_SESSION, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->interface->ioRead(this->driver));
}

/**********************************************************************************************************************************/
IoWrite *
ioSessionIoWrite(IoSession *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(IO_SESSION, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->interface->ioWrite(this->driver));
}

/**********************************************************************************************************************************/
IoSessionRole
ioSessionRole(const IoSession *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(IO_SESSION, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->interface->role(this->driver));
}

/**********************************************************************************************************************************/
String *
ioSessionToLog(const IoSession *this)
{
    return strNewFmt(
        "{type: %s, role: %s, driver: %s}", strZ(*this->interface->type),
        ioSessionRole(this) == ioSessionRoleClient ? "client" : "server", strZ(this->interface->toLog(this->driver)));
}
