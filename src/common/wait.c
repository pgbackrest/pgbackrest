/***********************************************************************************************************************************
Wait Handler
***********************************************************************************************************************************/
#include "common/debug.h"
#include "common/memContext.h"
#include "common/time.h"
#include "common/wait.h"

/***********************************************************************************************************************************
Contains information about the wait handler
***********************************************************************************************************************************/
struct Wait
{
    MemContext *memContext;                                         // Context that contains the wait handler
    TimeMSec waitTime;                                              // Total time to wait (in usec)
    TimeMSec sleepTime;                                             // Next sleep time (in usec)
    TimeMSec sleepPrevTime;                                         // Previous time slept (in usec)
    TimeMSec beginTime;                                             // Time the wait began (in epoch usec)
};

/***********************************************************************************************************************************
New wait handler
***********************************************************************************************************************************/
Wait *
waitNew(double waitTime)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(DOUBLE, waitTime);

        FUNCTION_DEBUG_ASSERT(waitTime >= 0.1 && waitTime <= 999999.0);
    FUNCTION_DEBUG_END();

    // Allocate wait object
    Wait *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("wait")
    {
        // Create object
        this = memNew(sizeof(Wait));
        this->memContext = MEM_CONTEXT_NEW();

        // Store time
        this->waitTime = (TimeMSec)(waitTime * MSEC_PER_SEC);

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

    FUNCTION_DEBUG_RESULT(WAIT, this);
}

/***********************************************************************************************************************************
Wait and return whether the caller has more time left
***********************************************************************************************************************************/
bool
waitMore(Wait *this)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(WAIT, this);

        FUNCTION_DEBUG_ASSERT(this != NULL);
    FUNCTION_DEBUG_END();

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
        }
        // Else set sleep to zero so next call will return false
        else
            this->sleepTime = 0;

        // Need to wait more
        result = true;
    }

    FUNCTION_DEBUG_RESULT(BOOL, result);
}

/***********************************************************************************************************************************
Free the wait
***********************************************************************************************************************************/
void
waitFree(Wait *this)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(WAIT, this);
    FUNCTION_DEBUG_END();

    if (this != NULL)
        memContextFree(this->memContext);

    FUNCTION_DEBUG_RESULT_VOID();
}
