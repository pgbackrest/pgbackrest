/***********************************************************************************************************************************
Test Stack Trace Handler
***********************************************************************************************************************************/
#include <assert.h>

#include "common/harnessStackTrace.h"

#ifdef HAVE_LIBBACKTRACE

static FN_NO_RETURN void
testStackTraceError3(void)
{
    stackTracePush("module/common/stackTraceTest.c", "testStackTraceError3", logLevelTrace);
    THROW(FormatError, "test error");
}

static FN_NO_RETURN void
testStackTraceError2(void)
{
    testStackTraceError3();
}

static FN_NO_RETURN void
testStackTraceError1(void)
{
    stackTracePush("src/file1.c", "testStackTraceError1", logLevelDebug);
    testStackTraceError2();
}

static FN_NO_RETURN void
testStackTraceError5(void)
{
    THROW_FMT(FormatError, "test error fmt");
}

static FN_NO_RETURN void
testStackTraceError4(void)
{
    stackTracePush("file4.c", "testStackTraceError4", logLevelTrace);
    testStackTraceError5();
}

#endif

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
static void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    // *****************************************************************************************************************************
    if (testBegin("stackTraceFmt()"))
    {
        char buffer[8];

        TEST_RESULT_UINT(stackTraceFmt(buffer, 8, 0, "%s", "1234567"), 7, "fill buffer");
        TEST_RESULT_Z(buffer, "1234567", "    check buffer");
        TEST_RESULT_UINT(stackTraceFmt(buffer, 8, 7, "%s", "1234567"), 7, "try to fill buffer - at end");
        TEST_RESULT_Z(buffer, "1234567", "    check buffer is unmodified");
        TEST_RESULT_UINT(stackTraceFmt(buffer, 8, 8, "%s", "1234567"), 7, "try to fill buffer - past end");
        TEST_RESULT_Z(buffer, "1234567", "    check buffer is unmodified");
    }

    // *****************************************************************************************************************************
    if (testBegin("libBackTrace"))
    {
#ifdef HAVE_LIBBACKTRACE
        // *************************************************************************************************************************
        TEST_TITLE("empty error function");

        TEST_RESULT_VOID(stackTraceBackErrorCallback(NULL, NULL, 0), "call error");

        // *************************************************************************************************************************
        TEST_TITLE("missing backtrace data");

        StackTraceBackData data =
        {
            .firstCall = true,
        };

        TEST_RESULT_INT(stackTraceBackCallback(&data, 0, NULL, 0, NULL), true, "all null, first call");

        data.firstCall = false;

        TEST_RESULT_INT(stackTraceBackCallback(&data, 0, "fileName", 0, NULL), false, "two null");
        TEST_RESULT_INT(stackTraceBackCallback(&data, 0, "fileName", 2, NULL), false, "one null");

        // Note that it is possible for libbacktrace to be present but not have any debug symbols to work with so handle that by
        // looking for an alternative error. However this will not work when coverage is required.
        // *************************************************************************************************************************
        TEST_TITLE("backtrace data");

        TRY_BEGIN()
        {
            testStackTraceError1();
        }
        CATCH_ANY()
        {
            TRY_BEGIN()
            {
                char buffer[4096];
                snprintf(buffer, sizeof(buffer), "%s", errorStackTrace());

                if (strstr(buffer, ":testRun:") != NULL)
                    memcpy(strstr(buffer, ":testRun:") + 9, "XX", 2);

                if (strstr(buffer, ":main:") != NULL)
                    memcpy(strstr(buffer, ":main:") + 6, "XXX", 3);

                TEST_RESULT_Z(
                    buffer,
                    "module/common/stackTraceTest.c:testStackTraceError3:14:(trace log level required for parameters)\n"
                    "module/common/stackTraceTest.c:testStackTraceError2:20:(no parameters available)\n"
                    "file1.c:testStackTraceError1:27:(debug log level required for parameters)\n"
                    "module/common/stackTraceTest.c:testRun:XX:(no parameters available)\n"
                    "../test.c:main:XXX:(no parameters available)",
                    "check stack trace");
            }
            CATCH(TestError)
            {
                hrnTestResultEnd();

                TRY_BEGIN()
                {
                    testStackTraceError1();
                }
                CATCH_ANY()
                {
                    TEST_RESULT_Z(
                        errorStackTrace(),
                        "module/common/stackTraceTest.c:testStackTraceError3:14:(trace log level required for parameters)\n"
                        "file1.c:testStackTraceError1:(debug log level required for parameters)",
                        "check stack trace");
                }
                TRY_END();
            }
            TRY_END();
        }
        TRY_END();

        TRY_BEGIN()
        {
            testStackTraceError4();
        }
        CATCH_ANY()
        {
            TRY_BEGIN()
            {
                char buffer[4096];
                snprintf(buffer, sizeof(buffer), "%s", errorStackTrace());

                if (strstr(buffer, ":testRun:") != NULL)
                    memcpy(strstr(buffer, ":testRun:") + 9, "XXX", 3);

                if (strstr(buffer, ":main:") != NULL)
                    memcpy(strstr(buffer, ":main:") + 6, "XXX", 3);

                TEST_RESULT_Z(
                    buffer,
                    "module/common/stackTraceTest.c:testStackTraceError5:33:(no parameters available)\n"
                    "file4.c:testStackTraceError4:40:(trace log level required for parameters)\n"
                    "module/common/stackTraceTest.c:testRun:XXX:(no parameters available)\n"
                    "../test.c:main:XXX:(no parameters available)",
                    "check stack trace");
            }
            CATCH(TestError)
            {
                hrnTestResultEnd();

                TRY_BEGIN()
                {
                    testStackTraceError4();
                }
                CATCH_ANY()
                {
                    TEST_RESULT_Z(
                        errorStackTrace(),
                        "module/common/stackTraceTest.c:testStackTraceError5:33:(test build required for parameters)\n"
                        "    ... function(s) omitted ...\n"
                        "file4.c:testStackTraceError4:(trace log level required for parameters)",
                        "check stack trace");
                }
                TRY_END();
            }
            TRY_END();
        }
        TRY_END();
#endif
    }

    // *****************************************************************************************************************************
    if (testBegin("stackTraceTestStart(), stackTraceTestStop(), and stackTraceTest()"))
    {
#ifdef DEBUG
        assert(stackTraceTest());

        stackTraceTestStop();
        assert(!stackTraceTest());

        stackTraceTestStart();
        assert(stackTraceTest());

        stackTraceLocal.stackSize++;
        stackTraceTestFileLineSet(888);
        assert(stackTraceLocal.stack[stackTraceLocal.stackSize - 1].fileLine == 888);
        stackTraceLocal.stackSize--;
#endif
    }

    // *****************************************************************************************************************************
    if (testBegin("stackTracePush(), stackTracePop(), and stackTraceClean()"))
    {
        char buffer[4096];

#ifdef HAVE_LIBBACKTRACE
        // Disable backtrace to make sure default code is called
        hrnStackTraceBackShimInstall();
#endif

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("check size of StackTraceData");

        TEST_RESULT_UINT(sizeof(StackTraceData), TEST_64BIT() ? 48 : 32, "check");

        TEST_ERROR(stackTracePop("file1", "function1", false), AssertError, "assertion 'stackTraceLocal.stackSize > 0' failed");

        assert(stackTracePush("file1", "function1", logLevelDebug) == logLevelDebug);
        assert(stackTraceLocal.stackSize == 1);

        TEST_ERROR(
            stackTracePop("file2", "function2", false), AssertError,
            "popping file2:function2 but expected file1:function1");

        assert(stackTracePush("file1", "function1", logLevelDebug) == logLevelDebug);

        TEST_ERROR(
            stackTracePop("file1", "function2", false), AssertError,
            "popping file1:function2 but expected file1:function1");

        TRY_BEGIN()
        {
            assert(stackTracePush("file1.c", "function1", logLevelDebug) == logLevelDebug);
            stackTraceParamLog();
            assert(strcmp(stackTraceParam(), "void") == 0);

            stackTraceToZ(buffer, sizeof(buffer), "file1.c", "function2", 99);

            TEST_RESULT_Z(
                buffer,
                "file1.c:function2:99:(test build required for parameters)\n"
                "    ... function(s) omitted ...\n"
                "file1.c:function1:(void)",
                "    check stack trace");

            assert(stackTracePush("file1.c", "function2", logLevelTrace) == logLevelTrace);
            stackTraceLocal.stack[stackTraceLocal.stackSize - 2].fileLine = 7777;
            assert(strcmp(stackTraceParam(), "trace log level required for parameters") == 0);
            stackTraceLocal.stack[stackTraceLocal.stackSize - 1].functionLogLevel = logLevelDebug;

            TRY_BEGIN()
            {
                // Function with one param
                assert(stackTracePush("file2.c", "function2", logLevelDebug) == logLevelDebug);
                stackTraceLocal.stack[stackTraceLocal.stackSize - 2].fileLine = 7777;

                stackTraceParamAdd((size_t)snprintf(stackTraceParamBuffer("param1"), STACK_TRACE_PARAM_MAX, "value1"));
                stackTraceParamLog();
                assert(strcmp(stackTraceParam(), "param1: value1") == 0);

                // Function with multiple params
                assert(stackTracePush("file3.c", "function3", logLevelTrace) == logLevelTrace);
                stackTraceLocal.stack[stackTraceLocal.stackSize - 2].fileLine = 7777;

                stackTraceParamLog();
                stackTraceParamAdd((size_t)snprintf(stackTraceParamBuffer("param1"), STACK_TRACE_PARAM_MAX, "value1"));
                stackTraceParamAdd((size_t)snprintf(stackTraceParamBuffer("param2"), STACK_TRACE_PARAM_MAX, "value2"));
                assert(strcmp(stackTraceParam(), "param1: value1, param2: value2") == 0);

                // Calculate exactly where the buffer will overflow (4 is for the separators)
                size_t bufferOverflow =
                    sizeof(stackTraceLocal.functionParamBuffer) - (STACK_TRACE_PARAM_MAX * 2) - strlen("param1") - 4 -
                    (size_t)(stackTraceLocal.stack[stackTraceLocal.stackSize - 1].param - stackTraceLocal.functionParamBuffer);

                // Munge the previous previous param in the stack so that the next one will just barely fit
                stackTraceLocal.stack[stackTraceLocal.stackSize - 1].paramSize = bufferOverflow - 1;

                assert(stackTracePush("src/file4.c", "function4", logLevelDebug) == logLevelTrace);
                stackTraceLocal.stack[stackTraceLocal.stackSize - 2].fileLine = 7777;
                stackTraceParamLog();
                assert(stackTraceLocal.stackSize == 5);

                // This param will fit exactly
                stackTraceParamAdd((size_t)snprintf(stackTraceParamBuffer("param1"), STACK_TRACE_PARAM_MAX, "value1"));
                assert(strcmp(stackTraceParam(), "param1: value1") == 0);

                // But when we increment the param pointer by one there will be overflow
                stackTraceLocal.stack[stackTraceLocal.stackSize - 1].param += 1;
                stackTraceLocal.stack[stackTraceLocal.stackSize - 1].paramSize = 0;
                stackTraceParamAdd((size_t)snprintf(stackTraceParamBuffer("param1"), STACK_TRACE_PARAM_MAX, "value1"));
                assert(strcmp(stackTraceParam(), "buffer full - parameters not available") == 0);

                stackTraceToZ(buffer, sizeof(buffer), "../pgbackrest/src/file4.c", "function4", 99);

                TEST_RESULT_Z(
                    buffer,
                    "file4.c:function4:99:(buffer full - parameters not available)\n"
                    "file3.c:function3:7777:(param1: value1, param2: value2)\n"
                    "file2.c:function2:7777:(param1: value1)\n"
                    "file1.c:function2:7777:(debug log level required for parameters)\n"
                    "file1.c:function1:7777:(void)",
                    "stack trace");

                stackTraceToZ(buffer, sizeof(buffer), "file5.c", "function4", 99);

                TEST_RESULT_Z(
                    buffer,
                    "file5.c:function4:99:(test build required for parameters)\n"
                    "    ... function(s) omitted ...\n"
                    "file4.c:function4:(buffer full - parameters not available)\n"
                    "file3.c:function3:7777:(param1: value1, param2: value2)\n"
                    "file2.c:function2:7777:(param1: value1)\n"
                    "file1.c:function2:7777:(debug log level required for parameters)\n"
                    "file1.c:function1:7777:(void)",
                    "stack trace");

                stackTracePop("src/file4.c", "function4", false);
                assert(stackTraceLocal.stackSize == 4);

                // Check that stackTracePop() works with test tracing
                stackTracePush("file_test.c", "function_test", logLevelDebug);
                stackTracePop("file_test.c", "function_test", true);

                // Check that stackTracePop() does nothing when test tracing is disabled
                stackTraceTestStop();
                stackTracePop("bogus.c", "bogus", true);
                stackTraceTestStart();

                THROW(ConfigError, "test");
            }
            CATCH(ConfigError)
            {
                // Ignore the error since we are just testing stack cleanup
            }
            TRY_END();

            assert(stackTraceLocal.stackSize == 2);
            THROW(ConfigError, "test");
        }
        CATCH(ConfigError)
        {
            // Ignore the error since we are just testing stack cleanup
        }
        TRY_END();

        assert(stackTraceLocal.stackSize == 0);

#ifdef HAVE_LIBBACKTRACE
        hrnStackTraceBackShimUninstall();
#endif
    }

    FUNCTION_HARNESS_RETURN_VOID();
}
