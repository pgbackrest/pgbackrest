/***********************************************************************************************************************************
Test Object Helper Macros
***********************************************************************************************************************************/
#include "common/macro.h"

/***********************************************************************************************************************************
TestObject Types
***********************************************************************************************************************************/
typedef struct TestObject
{
    bool data;                                                      // Test data
} TestObject;

/***********************************************************************************************************************************
Standard object methods
***********************************************************************************************************************************/
static TestObject *
testObjectMove(TestObject *this, MemContext *parentNew)
{
    return objMove(this, parentNew);
}

static void
testObjectFree(TestObject *this)
{
    objFree(this);
}

/**********************************************************************************************************************************/
static TestObject *
testObjectNew(void)
{
    OBJ_NEW_BEGIN(TestObject, .childQty = 1)
    {
        *this = (TestObject)
        {
            .data = true,
        };
    }
    OBJ_NEW_END();

    return this;
}

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
static void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    // *****************************************************************************************************************************
    if (testBegin("OBJECT_DEFINE*()"))
    {
        TEST_TITLE("object with mem context and allocation together");

        TestObject *testObject = NULL;

        MEM_CONTEXT_TEMP_BEGIN()
        {
            TEST_ASSIGN(testObject, testObjectNew(), "new test object");
            TEST_RESULT_VOID(testObjectMove(testObject, memContextPrior()), "move object to parent context");
            TEST_RESULT_VOID(testObjectMove(NULL, memContextPrior()), "move null object");
        }
        MEM_CONTEXT_TEMP_END();

        TEST_RESULT_PTR(objMemContext(testObject), memContextFromAllocExtra(testObject), "mem context");
        TEST_RESULT_VOID(testObjectFree(testObject), "free object");
        TEST_RESULT_VOID(testObjectFree(NULL), "free null object");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("move object to interface");

        TestObject *testInterface = NULL;

        TEST_ASSIGN(testInterface, testObjectNew(), "new interface object");
        TEST_ASSIGN(testObject, testObjectNew(), "new test object");

        // Only one of these can success since the interface only accepts one child
        TEST_RESULT_VOID(objMoveToInterface(testObject, testInterface, objMemContext(testObject)), "no move to interface");
        TEST_RESULT_VOID(objMoveToInterface(testObject, testInterface, memContextCurrent()), "move to interface");
    }

    FUNCTION_HARNESS_RETURN_VOID();
}
