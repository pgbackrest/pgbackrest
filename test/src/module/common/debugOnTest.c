/***********************************************************************************************************************************
Test Debug Macros and Routines
***********************************************************************************************************************************/
#include "common/log.h"

#include "common/harnessLog.h"
#include "common/macro.h"

static void
testFunction3(void)
{
    FUNCTION_TEST_VOID();
    FUNCTION_TEST_RETURN_VOID();
}

static void
testFunction2(void)
{
    FUNCTION_LOG_VOID(logLevelTrace);

    testFunction3();

    FUNCTION_LOG_RETURN_VOID();
}

static int
testFunction1(
    int paramInt, bool paramBool, bool *paramBoolP, bool **paramBoolPP, void *paramVoidP, double paramDouble, mode_t paramMode)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(INT, paramInt);
        FUNCTION_LOG_PARAM(BOOL, paramBool);
        FUNCTION_LOG_PARAM_P(BOOL, paramBoolP);
        FUNCTION_LOG_PARAM_PP(BOOL, paramBoolPP);
        FUNCTION_LOG_PARAM_P(VOID, paramVoidP);
        FUNCTION_LOG_PARAM(DOUBLE, paramDouble);
        FUNCTION_LOG_PARAM(MODE, paramMode);
    FUNCTION_LOG_END();

    testFunction2();

    FUNCTION_LOG_RETURN(INT, 1);
}

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    // *****************************************************************************************************************************
    if (testBegin("objToLog(), ptrToLog, and strzToLog()"))
    {
        char buffer[STACK_TRACE_PARAM_MAX];

        TEST_RESULT_UINT(objToLog(NULL, "Object", buffer, 4), 4, "truncated null");
        TEST_RESULT_Z(buffer, "nul", "    check truncated null");

        TEST_RESULT_UINT(objToLog(NULL, "Object", buffer, sizeof(buffer)), 4, "full null");
        TEST_RESULT_Z(buffer, "null", "    check full null");

        TEST_RESULT_UINT(objToLog((void *)1, "Object", buffer, 4), 8, "truncated object");
        TEST_RESULT_Z(buffer, "{Ob", "    check truncated object");

        TEST_RESULT_UINT(objToLog((void *)1, "Object", buffer, sizeof(buffer)), 8, "full object");
        TEST_RESULT_Z(buffer, "{Object}", "    check full object");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_UINT(ptrToLog(NULL, "char *", buffer, 4), 4, "truncated null");
        TEST_RESULT_Z(buffer, "nul", "    check truncated null");

        TEST_RESULT_UINT(ptrToLog(NULL, "char *", buffer, sizeof(buffer)), 4, "full null");
        TEST_RESULT_Z(buffer, "null", "    check full null");

        TEST_RESULT_UINT(ptrToLog((void *)1, "char *", buffer, 4), 8, "truncated pointer");
        TEST_RESULT_Z(buffer, "(ch", "    check truncated pointer");

        TEST_RESULT_UINT(ptrToLog((void *)1, "char *", buffer, sizeof(buffer)), 8, "full pointer");
        TEST_RESULT_Z(buffer, "(char *)", "    check full pointer");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_UINT(strzToLog(NULL, buffer, 4), 4, "truncated null");
        TEST_RESULT_Z(buffer, "nul", "    check truncated null");

        TEST_RESULT_UINT(strzToLog(NULL, buffer, sizeof(buffer)), 4, "full null");
        TEST_RESULT_Z(buffer, "null", "    check full null");

        TEST_RESULT_UINT(strzToLog("test", buffer, 4), 6, "truncated string");
        TEST_RESULT_Z(buffer, "\"te", "    check truncated string");

        TEST_RESULT_UINT(strzToLog("test2", buffer, sizeof(buffer)), 7, "full string");
        TEST_RESULT_Z(buffer, "\"test2\"", "    check full string");
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
        const char *debugUnitExtern = STRINGIFY(DEBUG_UNIT_EXTERN);
        TEST_RESULT_Z(debugUnitExtern, "", "DEBUG_UNIT_EXTERN is blank (extern)");
    }

    // *****************************************************************************************************************************
    if (testBegin("FUNCTION_DEBUG() and FUNCTION_LOG_RETURN()"))
    {
        harnessLogLevelSet(logLevelTrace);
        testFunction1(99, false, NULL, NULL, NULL, 1.17, 0755);

        harnessLogResult(
            "P00  DEBUG:     " TEST_PGB_PATH "/test/src/module/common/debugOnTest::testFunction1: (paramInt: 99, paramBool: false,"
                " paramBoolP: null, paramBoolPP: null, paramVoidP: null, paramDouble: 1.17, paramMode: 0755)\n"
            "P00  TRACE:         " TEST_PGB_PATH "/test/src/module/common/debugOnTest::testFunction2: (void)\n"
            "P00  TRACE:         " TEST_PGB_PATH "/test/src/module/common/debugOnTest::testFunction2: => void\n"
            "P00  DEBUG:     " TEST_PGB_PATH "/test/src/module/common/debugOnTest::testFunction1: => 1");

        // -------------------------------------------------------------------------------------------------------------------------
        bool testBool = true;
        bool *testBoolP = &testBool;
        bool **testBoolPP = &testBoolP;
        void *testVoidP = NULL;

        testFunction1(99, false, testBoolP, testBoolPP, testVoidP, 1.17, 0755);

        harnessLogResult(
            "P00  DEBUG:     " TEST_PGB_PATH "/test/src/module/common/debugOnTest::testFunction1: (paramInt: 99, paramBool: false,"
                " paramBoolP: *true, paramBoolPP: **true, paramVoidP: null, paramDouble: 1.17, paramMode: 0755)\n"
            "P00  TRACE:         " TEST_PGB_PATH "/test/src/module/common/debugOnTest::testFunction2: (void)\n"
            "P00  TRACE:         " TEST_PGB_PATH "/test/src/module/common/debugOnTest::testFunction2: => void\n"
            "P00  DEBUG:     " TEST_PGB_PATH "/test/src/module/common/debugOnTest::testFunction1: => 1");

        testBoolP = NULL;
        testVoidP = (void *)1;

        testFunction1(99, false, testBoolP, testBoolPP, testVoidP, 1.17, 0755);

        harnessLogResult(
            "P00  DEBUG:     " TEST_PGB_PATH "/test/src/module/common/debugOnTest::testFunction1: (paramInt: 99, paramBool: false,"
                " paramBoolP: null, paramBoolPP: *null, paramVoidP: *void, paramDouble: 1.17, paramMode: 0755)\n"
            "P00  TRACE:         " TEST_PGB_PATH "/test/src/module/common/debugOnTest::testFunction2: (void)\n"
            "P00  TRACE:         " TEST_PGB_PATH "/test/src/module/common/debugOnTest::testFunction2: => void\n"
            "P00  DEBUG:     " TEST_PGB_PATH "/test/src/module/common/debugOnTest::testFunction1: => 1");

        // -------------------------------------------------------------------------------------------------------------------------
        harnessLogLevelReset();
        testFunction1(55, true, NULL, NULL, NULL, 0.99, 0755);

        harnessLogResult("");
    }

    FUNCTION_HARNESS_RESULT_VOID();
}
