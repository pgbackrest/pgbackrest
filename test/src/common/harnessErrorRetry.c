/***********************************************************************************************************************************
Error Retry Message Harness
***********************************************************************************************************************************/
#include "build.auto.h"

#include "common/harnessDebug.h"
#include "common/harnessErrorRetry.h"

/***********************************************************************************************************************************
Include shimmed C modules
***********************************************************************************************************************************/
{[SHIM_MODULE]}

static struct
{
    bool detailEnable;                                              // Error retry details enabled (not the default)
} hrnErrorRetryLocal;

/**********************************************************************************************************************************/
void
errRetryAdd(ErrorRetry *const this)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(ERROR_RETRY, this);
    FUNCTION_HARNESS_END();

    if (!hrnErrorRetryLocal.detailEnable)
    {
        if (this->messageLast == NULL)
        {
            MEM_CONTEXT_OBJ_BEGIN(this)
            {
                this->pub.type = errorType();
                this->pub.message = strCatZ(strNew(), errorMessage());
                this->messageLast = strNewZ(errorMessage());
            }
            MEM_CONTEXT_OBJ_END();
        }
        else if (strEq(this->pub.message, this->messageLast))
            strCatFmt(this->pub.message, "\n[RETRY DETAIL OMITTED]");
    }
    else
        errRetryAdd_SHIMMED(this);

    FUNCTION_HARNESS_RETURN_VOID();
}

/**********************************************************************************************************************************/
void
hrnErrorRetryDetailEnable(void)
{
    FUNCTION_HARNESS_VOID();

    hrnErrorRetryLocal.detailEnable = true;

    FUNCTION_HARNESS_RETURN_VOID();
}

void
hrnErrorRetryDetailDisable(void)
{
    FUNCTION_HARNESS_VOID();

    hrnErrorRetryLocal.detailEnable = false;

    FUNCTION_HARNESS_RETURN_VOID();
}
