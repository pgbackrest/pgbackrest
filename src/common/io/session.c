/***********************************************************************************************************************************
Io Session Interface
***********************************************************************************************************************************/
#include "build.auto.h"

#include "common/debug.h"
#include "common/io/session.h"
#include "common/log.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct IoSession
{
    IoSessionPub pub;                                               // Publicly accessible variables
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
    ASSERT(interface->type != 0);
    ASSERT(interface->close != NULL);
    ASSERT(interface->ioRead != NULL);
    ASSERT(interface->ioWrite != NULL);
    ASSERT(interface->role != NULL);
    ASSERT(interface->toLog != NULL);

    IoSession *this = memNew(sizeof(IoSession));

    *this = (IoSession)
    {
        .pub =
        {
            .memContext = memContextCurrent(),
            .driver = driver,
            .interface = interface,
        },
    };

    FUNCTION_LOG_RETURN(IO_SESSION, this);
}

/**********************************************************************************************************************************/
void
ioSessionAuthenticatedSet(IoSession *const this, const bool authenticated)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(IO_SESSION, this);
        FUNCTION_TEST_PARAM(BOOL, authenticated);
    FUNCTION_TEST_END();

    this->pub.authenticated = authenticated;

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
int
ioSessionFd(IoSession *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(IO_SESSION, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->pub.interface->fd == NULL ? -1 : this->pub.interface->fd(this->pub.driver));
}

/**********************************************************************************************************************************/
void
ioSessionPeerNameSet(IoSession *const this, const String *const peerName)                                           // {vm_covered}
{
    FUNCTION_TEST_BEGIN();                                                                                          // {vm_covered}
        FUNCTION_TEST_PARAM(IO_SESSION, this);                                                                      // {vm_covered}
        FUNCTION_TEST_PARAM(STRING, peerName);                                                                      // {vm_covered}
    FUNCTION_TEST_END();                                                                                            // {vm_covered}

    MEM_CONTEXT_BEGIN(this->pub.memContext)                                                                         // {vm_covered}
    {
        this->pub.peerName = strDup(peerName);                                                                      // {vm_covered}
    }
    MEM_CONTEXT_END();                                                                                              // {vm_covered}

    FUNCTION_TEST_RETURN_VOID();                                                                                    // {vm_covered}
}                                                                                                                   // {vm_covered}

/**********************************************************************************************************************************/
String *
ioSessionToLog(const IoSession *this)
{
    return strNewFmt(
        "{type: %s, role: %s, driver: %s}", strZ(strIdToStr(this->pub.interface->type)), strZ(strIdToStr(ioSessionRole(this))),
         strZ(this->pub.interface->toLog(this->pub.driver)));
}
