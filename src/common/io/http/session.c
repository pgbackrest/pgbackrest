/***********************************************************************************************************************************
HTTP Session
***********************************************************************************************************************************/
#include "build.auto.h"

#include "common/debug.h"
#include "common/io/http/session.h"
#include "common/io/io.h"
#include "common/log.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct HttpSession
{
    HttpClient *httpClient;                                         // HTTP client
    IoSession *ioSession;                                           // IO session
};

/**********************************************************************************************************************************/
FN_EXTERN HttpSession *
httpSessionNew(HttpClient *const httpClient, IoSession *const ioSession)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(HTTP_CLIENT, httpClient);
        FUNCTION_LOG_PARAM(IO_SESSION, ioSession);
    FUNCTION_LOG_END();

    ASSERT(httpClient != NULL);
    ASSERT(ioSession != NULL);

    OBJ_NEW_BEGIN(HttpSession, .childQty = MEM_CONTEXT_QTY_MAX)
    {
        *this = (HttpSession)
        {
            .httpClient = httpClient,
            .ioSession = ioSessionMove(ioSession, memContextCurrent()),
        };
    }
    OBJ_NEW_END();

    FUNCTION_LOG_RETURN(HTTP_SESSION, this);
}

/**********************************************************************************************************************************/
FN_EXTERN void
httpSessionDone(HttpSession *const this)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(HTTP_SESSION, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    httpClientReuse(this->httpClient, this);

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
FN_EXTERN IoRead *
httpSessionIoRead(HttpSession *const this, const HttpSessionIoReadParam param)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(HTTP_SESSION, this);
        FUNCTION_TEST_PARAM(BOOL, param.ignoreUnexpectedEof);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(IO_READ, ioSessionIoReadP(this->ioSession, .ignoreUnexpectedEof = param.ignoreUnexpectedEof));
}

/**********************************************************************************************************************************/
FN_EXTERN IoWrite *
httpSessionIoWrite(HttpSession *const this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(HTTP_SESSION, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(IO_WRITE, ioSessionIoWrite(this->ioSession));
}
