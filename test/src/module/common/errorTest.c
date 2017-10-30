/***********************************************************************************************************************************
Test Error Handling
***********************************************************************************************************************************/

/***********************************************************************************************************************************
testTryRecurse - test to blow up try stack
***********************************************************************************************************************************/
int testTryRecurseTotal = 0;
bool testTryRecurseCatch = false;
bool testTryRecurseFinally = false;

void testTryRecurse()
{
    ERROR_TRY()
    {
        testTryRecurseTotal++;
        assert(errorContext.tryTotal == testTryRecurseTotal + 1);

        testTryRecurse();
    }
    ERROR_CATCH(MemoryError)
    {
        testTryRecurseCatch = true;                                 // {uncoverable - catch should never be executed}
    }
    ERROR_FINALLY()
    {
        testTryRecurseFinally = true;
    }
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
    if (testBegin("ERROR_TRY() with no errors"))
    {
        bool tryDone = false;
        bool catchDone = false;
        bool finallyDone = false;

        ERROR_TRY()
        {
            assert(errorContext.tryTotal == 1);
            tryDone = true;
        }
        ERROR_CATCH_ANY()
        {
            catchDone = true;                                       // {uncoverable - catch should never be executed}
        }
        ERROR_FINALLY()
        {
            assert(errorContext.tryList[1].state == errorStateFinal);
            finallyDone = true;
        }

        assert(tryDone);
        assert(!catchDone);
        assert(finallyDone);
        assert(errorContext.tryTotal == 0);
    }

    // -----------------------------------------------------------------------------------------------------------------------------
    if (testBegin("ERROR_TRY() with multiple catches"))
    {
        bool tryDone = false;
        bool catchDone = false;
        bool finallyDone = false;

        ERROR_TRY()
        {
            assert(errorContext.tryTotal == 1);

            ERROR_TRY()
            {
                assert(errorContext.tryTotal == 2);

                ERROR_TRY()
                {
                    assert(errorContext.tryTotal == 3);

                    ERROR_TRY()
                    {
                        assert(errorContext.tryTotal == 4);
                        tryDone = true;

                        ERROR_THROW(AssertError, BOGUS_STR);
                    }
                }
                ERROR_CATCH(AssertError)
                {
                    // Finally below should run even though this error has been rethrown
                    ERROR_RETHROW();
                }
                ERROR_FINALLY()
                {
                    finallyDone = true;
                }
            }
            ERROR_CATCH_ANY()
            {
                ERROR_RETHROW();
            }
        }
        ERROR_CATCH(MemoryError)
        {
            assert(false);                                              // {uncoverable - catch should never be executed}
        }
        ERROR_CATCH(RuntimeError)
        {
            assert(errorContext.tryTotal == 1);
            assert(errorContext.tryList[1].state == errorStateCatch);

            catchDone = true;
        }

        assert(tryDone);
        assert(catchDone);
        assert(finallyDone);
        assert(errorContext.tryTotal == 0);
    }

    // -----------------------------------------------------------------------------------------------------------------------------
    if (testBegin("too deep recursive TRY_ERROR()"))
    {
        bool tryDone = false;
        bool catchDone = false;
        bool finallyDone = false;

        ERROR_TRY()
        {
            tryDone = true;
            testTryRecurse();
        }
        ERROR_CATCH(AssertError)
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
        ERROR_FINALLY()
        {
            finallyDone = true;
        }

        assert(tryDone);
        assert(catchDone);
        assert(finallyDone);
        assert(errorContext.tryTotal == 0);

        // This is only ERROR_TRY_MAX - 1 because one try was used up by the wrapper above the recursive function
        assert(testTryRecurseTotal == ERROR_TRY_MAX - 1);
        assert(!testTryRecurseCatch);
        assert(testTryRecurseFinally);
    }
}
