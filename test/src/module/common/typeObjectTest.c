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

typedef struct TestObjectContext
{
    MemContext *memContext;                                         // Mem context
} TestObjectContext;

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

static TestObjectContext *
testObjectContextMove(TestObjectContext *this, MemContext *parentNew)
{
    return objMoveContext(this, parentNew);
}

static void
testObjectContextFree(TestObjectContext *this)
{
    objFreeContext(this);
}

/**********************************************************************************************************************************/
static TestObject *
testObjectNew(void)
{
    TestObject *this = NULL;

    OBJ_NEW_BEGIN(STRINGIFY(TestObject))
    {
        this = OBJ_NEW_ALLOC();

        *this = (TestObject)
        {
            .data = true,
        };
    }
    OBJ_NEW_END();

    return this;
}

/**********************************************************************************************************************************/
static TestObjectContext *
testObjectContextNew(void)
{
    TestObjectContext *this = NULL;

    OBJ_NEW_BEGIN(STRINGIFY(TestObjectContext))
    {
        this = OBJ_NEW_ALLOC();

        *this = (TestObjectContext)
        {
            .memContext = MEM_CONTEXT_NEW(),
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
        TEST_TITLE("Object with mem context and allocation together");

        TestObject *testObject = NULL;

        MEM_CONTEXT_TEMP_BEGIN()
        {
            TEST_ASSIGN(testObject, testObjectNew(), "new test object");
            TEST_RESULT_VOID(testObjectMove(testObject, memContextPrior()), "move object to parent context");
            TEST_RESULT_VOID(testObjectMove(NULL, memContextPrior()), "move null object");
        }
        MEM_CONTEXT_TEMP_END();

        TEST_RESULT_PTR(objMemContext(testObject), memContextFromAllocExtra(testObject), "mem context");
        TEST_RESULT_BOOL(objMemContextFreeing(testObject), false, "not freeing");
        TEST_RESULT_VOID(testObjectFree(testObject), "free object");
        TEST_RESULT_VOID(testObjectFree(NULL), "free null object");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("Object with mem context as first member of struct");

        TestObjectContext *testObjectContext = NULL;

        MEM_CONTEXT_TEMP_BEGIN()
        {
            TEST_ASSIGN(testObjectContext, testObjectContextNew(), "new test object");
            TEST_RESULT_VOID(testObjectContextMove(testObjectContext, memContextPrior()), "move object to parent context");
            TEST_RESULT_VOID(testObjectContextMove(NULL, memContextPrior()), "move null object");
        }
        MEM_CONTEXT_TEMP_END();

        TEST_RESULT_VOID(testObjectContextFree(testObjectContext), "free object");
        TEST_RESULT_VOID(testObjectContextFree(NULL), "free null object");
    }

    FUNCTION_HARNESS_RETURN_VOID();
}
