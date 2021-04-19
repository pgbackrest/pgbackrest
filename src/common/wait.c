/***********************************************************************************************************************************
Wait Handler
***********************************************************************************************************************************/
#include "build.auto.h"

#include "common/debug.h"
#include "common/log.h"
#include "common/memContext.h"
#include "common/wait.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct Wait
{
    WaitPub pub;                                                    // Publicly accessible variables
    TimeMSec waitTime;                                              // Total time to wait (in usec)
    TimeMSec sleepTime;                                             // Next sleep time (in usec)
    TimeMSec sleepPrevTime;                                         // Previous time slept (in usec)
    TimeMSec beginTime;                                             // Time the wait began (in epoch usec)
};

/**********************************************************************************************************************************/
Wait *
waitNew(TimeMSec waitTime)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(TIMEMSEC, waitTime);
    FUNCTION_LOG_END();

    ASSERT(waitTime <= 999999000);

    // Allocate wait object
    Wait *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("wait")
    {
        // Create object
        this = memNew(sizeof(Wait));

        *this = (Wait)
        {
            .pub =
            {
                .memContext = MEM_CONTEXT_NEW(),
                .remainTime = waitTime,
            },
            .waitTime = waitTime,
        };

        // Calculate first sleep time -- start with 1/10th of a second for anything >= 1 second
        if (this->waitTime >= MSEC_PER_SEC)
            this->sleepTime = MSEC_PER_SEC / 10;
        // Unless the wait time is really small -- in that case divide wait time by 10
        else
            this->sleepTime = this->waitTime / 10;

        // Get beginning time
        this->beginTime = timeMSec();
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_LOG_RETURN(WAIT, this);
}

/**********************************************************************************************************************************/
bool
waitMore(Wait *this)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(WAIT, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    bool result = false;

    // If sleep is 0 then the wait time has already ended
    if (this->sleepTime > 0)
    {
        // Sleep required amount
        sleepMSec(this->sleepTime);

        // Get the end time
        TimeMSec elapsedTime = timeMSec() - this->beginTime;

        // Is there more time to go?
        if (elapsedTime < this->waitTime)
        {
            // Calculate sleep time as a sum of current and last (a Fibonacci-type sequence)
            TimeMSec sleepNextTime = this->sleepTime + this->sleepPrevTime;

            // Make sure sleep time does not go beyond end time (this won't be negative because of the if condition above)
            if (sleepNextTime > this->waitTime - elapsedTime)
                sleepNextTime = this->waitTime - elapsedTime;

            // Store new sleep times
            this->sleepPrevTime = this->sleepTime;
            this->sleepTime = sleepNextTime;
            this->pub.remainTime = this->waitTime - elapsedTime;
        }
        // Else set sleep to zero so next call will return false
        else
            this->sleepTime = 0;

        // Need to wait more
        result = true;
    }

    FUNCTION_LOG_RETURN(BOOL, result);
}
