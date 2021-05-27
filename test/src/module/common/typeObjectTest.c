/***********************************************************************************************************************************
Test Object Helper Macros
***********************************************************************************************************************************/
#include "common/macro.h"

/***********************************************************************************************************************************
TestObject Type
***********************************************************************************************************************************/
typedef struct TestObject
{
    MemContext *memContext;                                         // Mem context
} TestObject;

/***********************************************************************************************************************************
Standard object methods
***********************************************************************************************************************************/
TestObject *
testObjectMove(TestObject *this, MemContext *parentNew)
{
    return objMove(this, parentNew);
}

void
testObjectFree(TestObject *this)
{
    objFree(this);
}

/**********************************************************************************************************************************/
TestObject *
testObjectNew(void)
{
    TestObject *this = NULL;

    MEM_CONTEXT_NEW_BEGIN(STRINGIFY(TestObject))
    {
        this = memNew(sizeof(TestObject));

        *this = (TestObject)
        {
            .memContext = MEM_CONTEXT_NEW(),
        };
    }
    MEM_CONTEXT_NEW_END();

    return this;
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
        TEST_RESULT_VOID(testObjectFree(NULL), "    free null object");
    }

    FUNCTION_HARNESS_RETURN_VOID();
}
