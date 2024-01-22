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
FN_EXTERN IoSession *
ioSessionNew(void *const driver, const IoSessionInterface *const interface)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
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

    OBJ_NEW_BEGIN(IoSession, .childQty = MEM_CONTEXT_QTY_MAX)
    {
        *this = (IoSession)
        {
            .pub =
            {
                .driver = objMoveToInterface(driver, this, memContextPrior()),
                .interface = interface,
            },
        };
    }
    OBJ_NEW_END();

    FUNCTION_LOG_RETURN(IO_SESSION, this);
}

/**********************************************************************************************************************************/
FN_EXTERN void
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
FN_EXTERN int
ioSessionFd(IoSession *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(IO_SESSION, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(INT, this->pub.interface->fd == NULL ? -1 : this->pub.interface->fd(this->pub.driver));
}

/**********************************************************************************************************************************/
FN_EXTERN void
ioSessionPeerNameSet(IoSession *const this, const String *const peerName)                                           // {vm_covered}
{
    FUNCTION_TEST_BEGIN();                                                                                          // {vm_covered}
        FUNCTION_TEST_PARAM(IO_SESSION, this);                                                                      // {vm_covered}
        FUNCTION_TEST_PARAM(STRING, peerName);                                                                      // {vm_covered}
    FUNCTION_TEST_END();                                                                                            // {vm_covered}

    MEM_CONTEXT_OBJ_BEGIN(this)                                                                                     // {vm_covered}
    {
        this->pub.peerName = strDup(peerName);                                                                      // {vm_covered}
    }
    MEM_CONTEXT_END();                                                                                              // {vm_covered}

    FUNCTION_TEST_RETURN_VOID();                                                                                    // {vm_covered}
}                                                                                                                   // {vm_covered}

/**********************************************************************************************************************************/
FN_EXTERN void
ioSessionToLog(const IoSession *const this, StringStatic *const debugLog)
{
    strStcCat(debugLog, "{type: ");
    strStcResultSizeInc(debugLog, strIdToLog(this->pub.interface->type, strStcRemains(debugLog), strStcRemainsSize(debugLog)));

    strStcCat(debugLog, ", role: ");
    strStcResultSizeInc(debugLog, strIdToLog(ioSessionRole(this), strStcRemains(debugLog), strStcRemainsSize(debugLog)));

    strStcCat(debugLog, ", driver: ");
    this->pub.interface->toLog(this->pub.driver, debugLog);
    strStcCatChr(debugLog, '}');
}
