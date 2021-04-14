/***********************************************************************************************************************************
HTTP Session
***********************************************************************************************************************************/
#include "build.auto.h"

#include "common/debug.h"
#include "common/io/http/session.h"
#include "common/io/io.h"
#include "common/log.h"
#include "common/memContext.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct HttpSession
{
    MemContext *memContext;                                         // Mem context
    HttpClient *httpClient;                                         // HTTP client
    IoSession *ioSession;                                           // IO session
};

/**********************************************************************************************************************************/
HttpSession *
httpSessionNew(HttpClient *httpClient, IoSession *ioSession)
{
    FUNCTION_LOG_BEGIN(logLevelDebug)
        FUNCTION_LOG_PARAM(HTTP_CLIENT, httpClient);
        FUNCTION_LOG_PARAM(IO_SESSION, ioSession);
    FUNCTION_LOG_END();

    ASSERT(httpClient != NULL);
    ASSERT(ioSession != NULL);

    HttpSession *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("HttpSession")
    {
        this = memNew(sizeof(HttpSession));

        *this = (HttpSession)
        {
            .memContext = MEM_CONTEXT_NEW(),
            .httpClient = httpClient,
            .ioSession = ioSessionMove(ioSession, memContextCurrent()),
        };
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_LOG_RETURN(HTTP_SESSION, this);
}

/**********************************************************************************************************************************/
void
httpSessionDone(HttpSession *this)
{
    FUNCTION_LOG_BEGIN(logLevelDebug)
        FUNCTION_LOG_PARAM(HTTP_SESSION, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    httpClientReuse(this->httpClient, this);

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
IoRead *
httpSessionIoRead(HttpSession *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(HTTP_SESSION, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(ioSessionIoRead(this->ioSession));
}

/**********************************************************************************************************************************/
IoWrite *
httpSessionIoWrite(HttpSession *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(HTTP_SESSION, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(ioSessionIoWrite(this->ioSession));
}
