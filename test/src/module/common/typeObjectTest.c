/***********************************************************************************************************************************
Test Object Helper Macros
***********************************************************************************************************************************/
#include "common/debug.h"
#include "common/log.h"
#include "common/memContext.h"

/***********************************************************************************************************************************
TestObject Type
***********************************************************************************************************************************/
#define TEST_OBJECT_TYPE                                            TestObject
#define TEST_OBJECT_PREFIX                                          testObject

typedef struct TestObject
{
    MemContext *memContext;                                         // Mem context
} TestObject;

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_TEST_OBJECT_TYPE                                                                                              \
    TestObject *
#define FUNCTION_LOG_TEST_OBJECT_FORMAT(value, buffer, bufferSize)                                                                 \
    objToLog(value, STRINGIFY(TestObject), buffer, bufferSize)

/***********************************************************************************************************************************
Free object
***********************************************************************************************************************************/
void testObjectFree(TestObject *this);

OBJECT_DEFINE_MOVE(TEST_OBJECT);
OBJECT_DEFINE_FREE(TEST_OBJECT);

/***********************************************************************************************************************************
Free object resource
***********************************************************************************************************************************/
bool testObjectFreeResourceCalled = false;

static void testObjectFreeResource(void *thisVoid);

OBJECT_DEFINE_FREE_RESOURCE_BEGIN(TEST_OBJECT, LOG, logLevelTrace)
{
    testObjectFreeResourceCalled = true;
}
OBJECT_DEFINE_FREE_RESOURCE_END(LOG);

/**********************************************************************************************************************************/
TestObject *
testObjectNew(void)
{
    FUNCTION_LOG_VOID(logLevelTrace);

    TestObject *this = NULL;

    MEM_CONTEXT_NEW_BEGIN(STRINGIFY(TestObject))
    {
        this = memNew(sizeof(TestObject));

        *this = (TestObject)
        {
            .memContext = MEM_CONTEXT_NEW(),
        };

        memContextCallbackSet(this->memContext, testObjectFreeResource, (void *)1);
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_LOG_RETURN(TEST_OBJECT, this);
}

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    // *****************************************************************************************************************************
    if (testBegin("OBJECT_DEFINE*()"))
    {
        TestObject *testObject = NULL;

        MEM_CONTEXT_TEMP_BEGIN()
        {
            TEST_ASSIGN(testObject, testObjectNew(), "new test object");
            TEST_RESULT_VOID(testObjectMove(testObject, memContextPrior()), "move object to parent context");
            TEST_RESULT_VOID(testObjectMove(NULL, memContextPrior()), "move null object");
        }
        MEM_CONTEXT_TEMP_END();

        TEST_RESULT_VOID(testObjectFree(testObject), "    free object");
        TEST_RESULT_BOOL(testObjectFreeResourceCalled, true, "    check callback");
    }

    FUNCTION_HARNESS_RETURN_VOID();
}
