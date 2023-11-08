/***********************************************************************************************************************************
Wait Handler
***********************************************************************************************************************************/
#include "build.auto.h"

#include "common/debug.h"
#include "common/log.h"
#include "common/wait.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct Wait
{
    TimeMSec waitTime;                                              // Total time to wait (in usec)
    TimeMSec sleepTime;                                             // Next sleep time (in usec)
    TimeMSec sleepPrevTime;                                         // Previous time slept (in usec)
    TimeMSec beginTime;                                             // Time the wait began (in epoch usec)
    unsigned int retry;                                             // Number of retries
};

/**********************************************************************************************************************************/
FN_EXTERN Wait *
waitNew(const TimeMSec waitTime)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(TIMEMSEC, waitTime);
    FUNCTION_LOG_END();

    ASSERT(waitTime <= 999999000);

    OBJ_NEW_BEGIN(Wait, .childQty = MEM_CONTEXT_QTY_MAX)
    {
        *this = (Wait)
        {
            .waitTime = waitTime,
            .retry = 2,
        };

        // Calculate first sleep time -- start with 1/10th of a second for anything >= 1 second
        if (this->waitTime >= MSEC_PER_SEC)
        {
            this->sleepTime = MSEC_PER_SEC / 10;
        }
        // Unless the wait time is really small -- in that case divide wait time by 10
        else
            this->sleepTime = this->waitTime / 10;

        // Get beginning time
        this->beginTime = timeMSec();
    }
    OBJ_NEW_END();

    FUNCTION_LOG_RETURN(WAIT, this);
}

/**********************************************************************************************************************************/
FN_EXTERN bool
waitMore(Wait *const this)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(WAIT, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    bool result = false;

    // If sleep is 0 then the wait time has already ended
    if (this->sleepTime > 0)
    {
        // Get the elapsed time
        const TimeMSec elapsedTime = timeMSec() - this->beginTime;

        // Is there more time to go?
        if (elapsedTime < this->waitTime)
        {
            // Calculate remaining sleep time
            const TimeMSec remainTime = this->waitTime - elapsedTime;

            // Calculate sleep time as a sum of current and last (a Fibonacci-type sequence)
            TimeMSec sleepTime = this->sleepTime + this->sleepPrevTime;

            // Make sure sleep time does not go beyond remaining time (this won't be negative because of the if condition above)
            if (sleepTime > remainTime)
                sleepTime = remainTime;

            // Sleep required amount
            sleepMSec(sleepTime);

            // Store new sleep times
            this->sleepPrevTime = this->sleepTime;
            this->sleepTime = sleepTime;
        }
        // Else are there retries left?
        else if (this->retry != 0)
        {
            // Sleep using the last calculated time
            sleepMSec(this->sleepTime);
        }
        // Else set sleep to zero so call will return false
        else
            this->sleepTime = 0;

        // Caller can continue processing
        if (this->sleepTime > 0)
        {
            // Decrement retries
            if (this->retry != 0)
                this->retry--;

            result = true;
        }
    }

    FUNCTION_LOG_RETURN(BOOL, result);
}
