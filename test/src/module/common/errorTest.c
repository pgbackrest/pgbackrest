/***********************************************************************************************************************************
Test Error Handling
***********************************************************************************************************************************/

/***********************************************************************************************************************************
testTryRecurse - test to blow up try stack
***********************************************************************************************************************************/
volatile int testTryRecurseTotal = 0;
bool testTryRecurseCatch = false;
bool testTryRecurseFinally = false;

void testTryRecurse()
{
    TRY_BEGIN()
    {
        testTryRecurseTotal++;
        assert(errorContext.tryTotal == testTryRecurseTotal + 1);

        testTryRecurse();
    }
    CATCH(MemoryError)
    {
        testTryRecurseCatch = true;                                 // {uncoverable - catch should never be executed}
    }
    FINALLY()
    {
        testTryRecurseFinally = true;
    }
    TRY_END();
}                                                                   // {uncoverable - function throws error, never returns}

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void testRun()
{
    // -----------------------------------------------------------------------------------------------------------------------------
    if (testBegin("check that try stack is initialized correctly"))
    {
        assert(errorContext.tryTotal == 0);
    }

    // -----------------------------------------------------------------------------------------------------------------------------
    if (testBegin("TRY with no errors"))
    {
        volatile bool tryDone = false;
        bool catchDone = false;
        bool finallyDone = false;

        TRY_BEGIN()
        {
            assert(errorContext.tryTotal == 1);
            tryDone = true;
        }
        CATCH_ANY()
        {
            catchDone = true;                                       // {uncoverable - catch should never be executed}
        }
        FINALLY()
        {
            assert(errorContext.tryList[1].state == errorStateFinal);
            finallyDone = true;
        }
        TRY_END();

        assert(tryDone);
        assert(!catchDone);
        assert(finallyDone);
        assert(errorContext.tryTotal == 0);
    }

    // -----------------------------------------------------------------------------------------------------------------------------
    if (testBegin("TRY with multiple catches"))
    {
        volatile bool tryDone = false;
        volatile bool catchDone = false;
        volatile bool finallyDone = false;

        TRY_BEGIN()
        {
            assert(errorContext.tryTotal == 1);

            TRY_BEGIN()
            {
                assert(errorContext.tryTotal == 2);

                TRY_BEGIN()
                {
                    assert(errorContext.tryTotal == 3);

                    TRY_BEGIN()
                    {
                        assert(errorContext.tryTotal == 4);
                        tryDone = true;

                        THROW(AssertError, BOGUS_STR);
                    }
                    TRY_END();
                }
                CATCH(AssertError)
                {
                    // Finally below should run even though this error has been rethrown
                    RETHROW();
                }
                FINALLY()
                {
                    finallyDone = true;
                }
                TRY_END();
            }
            CATCH_ANY()
            {
                RETHROW();
            }
            TRY_END();
        }
        CATCH(MemoryError)
        {
            assert(false);                                              // {uncoverable - catch should never be executed}
        }
        CATCH(RuntimeError)
        {
            assert(errorContext.tryTotal == 1);
            assert(errorContext.tryList[1].state == errorStateCatch);

            catchDone = true;
        }
        TRY_END();

        assert(tryDone);
        assert(catchDone);
        assert(finallyDone);
        assert(errorContext.tryTotal == 0);
    }

    // -----------------------------------------------------------------------------------------------------------------------------
    if (testBegin("too deep recursive TRY_ERROR()"))
    {
        volatile bool tryDone = false;
        bool catchDone = false;
        bool finallyDone = false;

        TRY_BEGIN()
        {
            tryDone = true;
            testTryRecurse();
        }
        CATCH(AssertError)
        {
            assert(errorCode() == AssertError.code);
            assert(errorFileName() != NULL);
            assert(errorFileLine() >= 1);
            assert(strcmp(errorMessage(), "too many nested try blocks") == 0);
            assert(strcmp(errorName(), AssertError.name) == 0);
            assert(errorType() == &AssertError);
            assert(errorTypeCode(errorType()) == AssertError.code);
            assert(strcmp(errorTypeName(errorType()), AssertError.name) == 0);
            catchDone = true;
        }
        FINALLY()
        {
            finallyDone = true;
        }
        TRY_END();

        assert(tryDone);
        assert(catchDone);
        assert(finallyDone);
        assert(errorContext.tryTotal == 0);

        // This is only ERROR_TRY_MAX - 1 because one try was used up by the wrapper above the recursive function
        assert(testTryRecurseTotal == ERROR_TRY_MAX - 1);
        assert(!testTryRecurseCatch);
        assert(testTryRecurseFinally);
    }

    // -----------------------------------------------------------------------------------------------------------------------------
    if (testBegin("THROW_CODE()"))
    {
        TRY_BEGIN()
        {
            THROW_CODE(25, "message");
        }
        CATCH_ANY()
        {
            assert(errorCode() == 25);
            assert(strcmp(errorMessage(), "message") == 0);
        }
        TRY_END();

        // -------------------------------------------------------------------------------------------------------------------------
        TRY_BEGIN()
        {
            THROW_CODE(777, "message");
        }
        CATCH_ANY()
        {
            assert(errorCode() == AssertError.code);
            assert(strcmp(errorMessage(), "could not find error type for code '777'") == 0);
        }
        TRY_END();
    }

    // -----------------------------------------------------------------------------------------------------------------------------
    if (testBegin("THROW_ON_SYS_ERROR()"))
    {
        THROW_ON_SYS_ERROR(0, AssertError, "message");

        TRY_BEGIN()
        {
            errno = E2BIG;
            THROW_ON_SYS_ERROR(1, AssertError, "message");
        }
        CATCH_ANY()
        {
            printf("%s\n", errorMessage());
            assert(errorCode() == AssertError.code);
            assert(strcmp(errorMessage(), "message: [7] Argument list too long") == 0);
        }
        TRY_END();
    }
}
