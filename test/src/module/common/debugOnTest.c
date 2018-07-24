/***********************************************************************************************************************************
Test Debug Macros and Routines
***********************************************************************************************************************************/
#include "common/log.h"

#include "common/harnessLog.h"

static void
testFunction3()
{
    FUNCTION_TEST_VOID();
    FUNCTION_TEST_RESULT_VOID();
}

static void
testFunction2()
{
    FUNCTION_DEBUG_VOID(logLevelTrace);

    testFunction3();

    FUNCTION_DEBUG_RESULT_VOID();
}

static int
testFunction1(int paramInt, bool paramBool, double paramDouble, mode_t paramMode)
{
    FUNCTION_DEBUG_BEGIN(logLevelDebug);
        FUNCTION_DEBUG_PARAM(INT, paramInt);
        FUNCTION_DEBUG_PARAM(BOOL, paramBool);
        FUNCTION_DEBUG_PARAM(DOUBLE, paramDouble);
        FUNCTION_DEBUG_PARAM(MODE, paramMode);
    FUNCTION_DEBUG_END();

    testFunction2();

    FUNCTION_DEBUG_RESULT(INT, 1);
}

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun()
{
    FUNCTION_HARNESS_VOID();

    // *****************************************************************************************************************************
    if (testBegin("objToLog(), ptrToLog, and strzToLog()"))
    {
        char buffer[STACK_TRACE_PARAM_MAX];

        TEST_RESULT_INT(objToLog(NULL, "Object", buffer, 4), 4, "truncated null");
        TEST_RESULT_STR(buffer, "nul", "    check truncated null");

        TEST_RESULT_INT(objToLog(NULL, "Object", buffer, sizeof(buffer)), 4, "full null");
        TEST_RESULT_STR(buffer, "null", "    check full null");

        TEST_RESULT_INT(objToLog((void *)1, "Object", buffer, 4), 8, "truncated object");
        TEST_RESULT_STR(buffer, "{Ob", "    check truncated object");

        TEST_RESULT_INT(objToLog((void *)1, "Object", buffer, sizeof(buffer)), 8, "full object");
        TEST_RESULT_STR(buffer, "{Object}", "    check full object");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_INT(ptrToLog(NULL, "char *", buffer, 4), 4, "truncated null");
        TEST_RESULT_STR(buffer, "nul", "    check truncated null");

        TEST_RESULT_INT(ptrToLog(NULL, "char *", buffer, sizeof(buffer)), 4, "full null");
        TEST_RESULT_STR(buffer, "null", "    check full null");

        TEST_RESULT_INT(ptrToLog((void *)1, "char *", buffer, 4), 8, "truncated pointer");
        TEST_RESULT_STR(buffer, "(ch", "    check truncated pointer");

        TEST_RESULT_INT(ptrToLog((void *)1, "char *", buffer, sizeof(buffer)), 8, "full pointer");
        TEST_RESULT_STR(buffer, "(char *)", "    check full pointer");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_INT(strzToLog(NULL, buffer, 4), 4, "truncated null");
        TEST_RESULT_STR(buffer, "nul", "    check truncated null");

        TEST_RESULT_INT(strzToLog(NULL, buffer, sizeof(buffer)), 4, "full null");
        TEST_RESULT_STR(buffer, "null", "    check full null");

        TEST_RESULT_INT(strzToLog("test", buffer, 4), 6, "truncated string");
        TEST_RESULT_STR(buffer, "\"te", "    check truncated string");

        TEST_RESULT_INT(strzToLog("test2", buffer, sizeof(buffer)), 7, "full string");
        TEST_RESULT_STR(buffer, "\"test2\"", "    check full string");
    }

    // *****************************************************************************************************************************
    if (testBegin("DEBUG"))
    {
#ifdef DEBUG
        bool debug = true;
#else
        bool debug = false;
#endif

        TEST_RESULT_BOOL(debug, true, "DEBUG is defined");
    }

    // *****************************************************************************************************************************
    if (testBegin("DEBUG_UNIT_EXTERN"))
    {
        const char *debugUnitExtern = MACRO_TO_STR(DEBUG_UNIT_EXTERN);
        TEST_RESULT_STR(debugUnitExtern, "", "DEBUG_UNIT_EXTERN is blank (extern)");
    }

    // *****************************************************************************************************************************
    if (testBegin("FUNCTION_DEBUG() and FUNCTION_DEBUG_RESULT()"))
    {
        harnessLogLevelSet(logLevelTrace);
        testFunction1(99, false, 1.17, 0755);

        harnessLogResult(
            "P00  DEBUG:     module/common/debugOnTest::testFunction1: (paramInt: 99, paramBool: false, paramDouble: 1.17"
                ", paramMode: 0755)\n"
            "P00  TRACE:         module/common/debugOnTest::testFunction2: (void)\n"
            "P00  TRACE:         module/common/debugOnTest::testFunction2: => void\n"
            "P00  DEBUG:     module/common/debugOnTest::testFunction1: => 1");
        harnessLogLevelReset();

        // -------------------------------------------------------------------------------------------------------------------------
        testFunction1(55, true, 0.99, 0755);

        harnessLogResult("");
    }

    FUNCTION_HARNESS_RESULT_VOID();
}
