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
String *
errRetryMessage(const ErrorRetry *const this)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(ERROR_RETRY, this);
    FUNCTION_HARNESS_END();

    ASSERT(this->message != NULL);

    String *result = NULL;

    if (!hrnErrorRetryLocal.detailEnable)
    {
        result = strCat(strNew(), this->message);

        if (lstSize(this->list) > 0)
            strCatZ(result, "\n[RETRY DETAIL OMITTED]");
    }
    else
        result = errRetryMessage_SHIMMED(this);

    FUNCTION_HARNESS_RETURN(STRING, result);
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
