/***********************************************************************************************************************************
Test Memory Contexts
***********************************************************************************************************************************/

/***********************************************************************************************************************************
testFree - test callback function
***********************************************************************************************************************************/
MemContext *memContextCallbackArgument = NULL;
bool testFreeThrow = false;

void
testFree(void *thisVoid)
{
    MemContext *this = thisVoid;

    TEST_RESULT_INT(this->state, memContextStateFreeing, "state should be freeing before memContextFree() in callback");
    memContextFree(this);
    TEST_RESULT_INT(this->state, memContextStateFreeing, "state should still be freeing after memContextFree() in callback");

    TEST_ERROR(memContextCallbackSet(this, testFree, this), AssertError, "cannot assign callback to inactive context");

    TEST_ERROR(memContextSwitch(this), AssertError, "cannot switch to inactive context");
    TEST_ERROR(memContextName(this), AssertError, "cannot get name for inactive context");

    memContextCallbackArgument = this;

    if (testFreeThrow)
        THROW(AssertError, "error in callback");
}

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    // *****************************************************************************************************************************
    if (testBegin("memAllocInternal(), memReAllocInternal(), and memFreeInternal()"))
    {
        // Test too large allocation -- only test this on 64-bit systems since 32-bit systems tend to work with any value that
        // valgrind will accept
        if (TEST_64BIT())
        {
            TEST_ERROR(memAllocInternal((size_t)5629499534213120, false), MemoryError, "unable to allocate 5629499534213120 bytes");
            TEST_ERROR(memFreeInternal(NULL), AssertError, "assertion 'buffer != NULL' failed");

            // Check that bad realloc is caught
            void *buffer = memAllocInternal(sizeof(size_t), false);
            TEST_ERROR(
                memReAllocInternal(buffer, sizeof(size_t), (size_t)5629499534213120, false), MemoryError,
                "unable to reallocate 5629499534213120 bytes");
            memFreeInternal(buffer);
        }

        // Normal memory allocation
        void *buffer = memAllocInternal(sizeof(size_t), false);
        buffer = memReAllocInternal(buffer, sizeof(size_t), sizeof(size_t) * 2, false);
        memFreeInternal(buffer);

        // Zeroed memory allocation
        unsigned char *buffer2 = memAllocInternal(sizeof(size_t), true);
        int expectedTotal = 0;

        for (unsigned int charIdx = 0; charIdx < sizeof(size_t); charIdx++)
            expectedTotal += buffer2[charIdx] == 0;

        TEST_RESULT_INT(expectedTotal, sizeof(size_t), "all bytes are 0");

        // Zeroed memory reallocation
        memset(buffer2, 0xC7, sizeof(size_t));

        buffer2 = memReAllocInternal(buffer2, sizeof(size_t), sizeof(size_t) * 2, true);

        expectedTotal = 0;

        for (unsigned int charIdx = 0; charIdx < sizeof(size_t); charIdx++)
            expectedTotal += buffer2[charIdx] == 0xC7;

        TEST_RESULT_INT(expectedTotal, sizeof(size_t), "all old bytes are filled");

        expectedTotal = 0;

        for (unsigned int charIdx = 0; charIdx < sizeof(size_t); charIdx++)
            expectedTotal += (buffer2 + sizeof(size_t))[charIdx] == 0;

        TEST_RESULT_INT(expectedTotal, sizeof(size_t), "all new bytes are 0");
        memFreeInternal(buffer2);
    }

    // *****************************************************************************************************************************
    if (testBegin("memContextNew() and memContextFree()"))
    {
        // Make sure top context was created
        TEST_RESULT_STR(memContextName(memContextTop()), "TOP", "top context should exist");
        TEST_RESULT_INT(memContextTop()->contextChildListSize, 0, "top context should init with zero children");
        TEST_RESULT_PTR(memContextTop()->contextChildList, NULL, "top context child list empty");

        // Current context should equal top context
        TEST_RESULT_PTR(memContextCurrent(), memContextTop(), "top context == current context");

        // Context name length errors
        TEST_ERROR(memContextNew(""), AssertError, "assertion 'name[0] != '\\0'' failed");

        MemContext *memContext = memContextNew("test1");
        TEST_RESULT_STR(memContextName(memContext), "test1", "test1 context name");
        TEST_RESULT_PTR(memContext->contextParent, memContextTop(), "test1 context parent is top");
        TEST_RESULT_INT(memContextTop()->contextChildListSize, MEM_CONTEXT_INITIAL_SIZE, "initial top context child list size");

        TEST_RESULT_PTR(memContextSwitch(memContext), memContextTop(), "switch returns top as old context");
        TEST_RESULT_PTR(memContext, memContextCurrent(), "current context is now test1");

        // Create enough mem contexts to use up the initially allocated block
        for (int contextIdx = 1; contextIdx < MEM_CONTEXT_INITIAL_SIZE; contextIdx++)
        {
            memContextSwitch(memContextTop());
            memContextNew("test-filler");
            TEST_RESULT_BOOL(
                memContextTop()->contextChildList[contextIdx]->state == memContextStateActive, true, "new context is active");
            TEST_RESULT_STR(memContextName(memContextTop()->contextChildList[contextIdx]), "test-filler", "new context name");
        }

        // This forces the child context array to grow
        memContextNew("test5");
        TEST_RESULT_INT(memContextTop()->contextChildListSize, MEM_CONTEXT_INITIAL_SIZE * 2, "increased child context list size");
        TEST_RESULT_UINT(memContextTop()->contextChildFreeIdx, MEM_CONTEXT_INITIAL_SIZE + 1, "check context free idx");

        // Free a context
        memContextFree(memContextTop()->contextChildList[1]);
        TEST_RESULT_BOOL(
            memContextTop()->contextChildList[1]->state == memContextStateActive, false, "child context inactive after free");
        TEST_RESULT_PTR(memContextCurrent(), memContextTop(), "current context is top");
        TEST_RESULT_UINT(memContextTop()->contextChildFreeIdx, 1, "check context free idx");

        // Create a new context and it should end up in the same spot
        memContextNew("test-reuse");
        TEST_RESULT_BOOL(
            memContextTop()->contextChildList[1]->state == memContextStateActive,
            true, "new context in same index as freed context is active");
        TEST_RESULT_STR(memContextName(memContextTop()->contextChildList[1]), "test-reuse", "new context name");
        TEST_RESULT_UINT(memContextTop()->contextChildFreeIdx, 2, "check context free idx");

        // Next context will be at the end
        memContextNew("test-at-end");
        TEST_RESULT_UINT(memContextTop()->contextChildFreeIdx, MEM_CONTEXT_INITIAL_SIZE + 2, "check context free idx");

        // Create a child context to test recursive free
        memContextSwitch(memContextTop()->contextChildList[MEM_CONTEXT_INITIAL_SIZE]);
        memContextNew("test-reuse");
        TEST_RESULT_PTR_NE(
            memContextTop()->contextChildList[MEM_CONTEXT_INITIAL_SIZE]->contextChildList, NULL, "context child list is allocated");
        TEST_RESULT_INT(
            memContextTop()->contextChildList[MEM_CONTEXT_INITIAL_SIZE]->contextChildListSize, MEM_CONTEXT_INITIAL_SIZE,
            "context child list initial size");

        TEST_ERROR(
            memContextFree(memContextTop()->contextChildList[MEM_CONTEXT_INITIAL_SIZE]),
            AssertError, "cannot free current context 'test5'");

        memContextSwitch(memContextTop());
        memContextFree(memContextTop()->contextChildList[MEM_CONTEXT_INITIAL_SIZE]);

        TEST_ERROR(
            memContextFree(memContextTop()->contextChildList[MEM_CONTEXT_INITIAL_SIZE]),
            AssertError, "cannot free inactive context");

        MemContext *noAllocation = memContextNew("empty");
        noAllocation->allocListSize = 0;
        free(noAllocation->allocList);
        TEST_RESULT_VOID(memContextFree(noAllocation), "free context with no allocations");
    }

    // *****************************************************************************************************************************
    if (testBegin("memContextAlloc(), memNew*(), memGrow(), and memFree()"))
    {
        memContextSwitch(memContextTop());
        memNew(sizeof(size_t));

        MemContext *memContext = memContextNew("test-alloc");
        memContextSwitch(memContext);

        for (int allocIdx = 0; allocIdx <= MEM_CONTEXT_ALLOC_INITIAL_SIZE; allocIdx++)
        {
            unsigned char *buffer = memNew(sizeof(size_t));

            // Check that the buffer is zeroed
            int expectedTotal = 0;

            for (unsigned int charIdx = 0; charIdx < sizeof(size_t); charIdx++)
                expectedTotal += buffer[charIdx] == 0;

            TEST_RESULT_INT(expectedTotal, sizeof(size_t), "all bytes are 0");

            TEST_RESULT_INT(
                memContextCurrent()->allocListSize,
                allocIdx == MEM_CONTEXT_ALLOC_INITIAL_SIZE ? MEM_CONTEXT_ALLOC_INITIAL_SIZE * 2 : MEM_CONTEXT_ALLOC_INITIAL_SIZE,
                "allocation list size");
        }


        unsigned char *buffer = memNewRaw(sizeof(size_t));

        // Grow memory
        memset(buffer, 0xFE, sizeof(size_t));
        buffer = memGrowRaw(buffer, sizeof(size_t) * 2);

        // Check that original portion of the buffer is preserved
        int expectedTotal = 0;

        for (unsigned int charIdx = 0; charIdx < sizeof(size_t); charIdx++)
            expectedTotal += buffer[charIdx] == 0xFE;

        TEST_RESULT_INT(expectedTotal, sizeof(size_t), "all bytes are 0xFE in original portion");

        // Free memory
        TEST_RESULT_UINT(memContextCurrent()->allocFreeIdx, MEM_CONTEXT_ALLOC_INITIAL_SIZE + 2, "check alloc free idx");
        TEST_RESULT_VOID(memFree(memContextCurrent()->allocList[0].buffer), "free allocation");
        TEST_ERROR(memFree(memContextCurrent()->allocList[0].buffer), AssertError, "unable to find allocation");
        TEST_RESULT_UINT(memContextCurrent()->allocFreeIdx, 0, "check alloc free idx");

        TEST_RESULT_VOID(memFree(memContextCurrent()->allocList[1].buffer), "free allocation");
        TEST_RESULT_UINT(memContextCurrent()->allocFreeIdx, 0, "check alloc free idx");

        TEST_RESULT_VOID(memNew(3), "new allocation");
        TEST_RESULT_UINT(memContextCurrent()->allocFreeIdx, 1, "check alloc free idx");

        TEST_RESULT_VOID(memNew(3), "new allocation");
        TEST_RESULT_UINT(memContextCurrent()->allocFreeIdx, 2, "check alloc free idx");

        TEST_RESULT_VOID(memNew(3), "new allocation");
        TEST_RESULT_UINT(memContextCurrent()->allocFreeIdx, MEM_CONTEXT_ALLOC_INITIAL_SIZE + 3, "check alloc free idx");

        TEST_ERROR(memFree(NULL), AssertError, "assertion 'buffer != NULL' failed");
        TEST_ERROR(memFree((void *)0x01), AssertError, "unable to find allocation");
        memFree(buffer);

        memContextSwitch(memContextTop());
        memContextFree(memContext);
    }

    // *****************************************************************************************************************************
    if (testBegin("memContextCallbackSet()"))
    {
        TEST_ERROR(
            memContextCallbackSet(memContextTop(), testFree, NULL), AssertError, "top context may not have a callback");

        MemContext *memContext = memContextNew("test-callback");
        memContextCallbackSet(memContext, testFree, memContext);
        TEST_ERROR(
            memContextCallbackSet(memContext, testFree, memContext), AssertError,
            "callback is already set for context 'test-callback'");

        // Clear and reset it
        memContextCallbackClear(memContext);
        memContextCallbackSet(memContext, testFree, memContext);

        memContextFree(memContext);
        TEST_RESULT_PTR(memContextCallbackArgument, memContext, "callback argument is context");

        // Now test with an error
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_ASSIGN(memContext, memContextNew("test-callback-error"), "new mem context");
        testFreeThrow = true;
        TEST_RESULT_VOID(memContextCallbackSet(memContext, testFree, memContext), "    set callback");
        TEST_ERROR(memContextFree(memContext), AssertError, "error in callback");
        TEST_RESULT_UINT(memContext->state, memContextStateFree, "    check that mem context was completely freed");
    }

    // *****************************************************************************************************************************
    if (testBegin("MEM_CONTEXT_BEGIN() and MEM_CONTEXT_END()"))
    {
        memContextSwitch(memContextTop());
        MemContext *memContext = memContextNew("test-block");

        // Check normal block
        MEM_CONTEXT_BEGIN(memContext)
        {
            TEST_RESULT_STR(memContextName(memContextCurrent()), "test-block", "context is now test-block");
        }
        MEM_CONTEXT_END();

        TEST_RESULT_STR(memContextName(memContextCurrent()), "TOP", "context is now top");

        // Check block that errors
        TEST_ERROR(
            MEM_CONTEXT_BEGIN(memContext)
            {
                TEST_RESULT_STR(memContextName(memContextCurrent()), "test-block", "context is now test-block");
                THROW(AssertError, "error in test block");
            }
            MEM_CONTEXT_END(),
            AssertError, "error in test block");

        TEST_RESULT_STR(memContextName(memContextCurrent()), "TOP", "context is now top");
    }

    // *****************************************************************************************************************************
    if (testBegin("MEM_CONTEXT_NEW_BEGIN() and MEM_CONTEXT_NEW_END()"))
    {
        // ------------------------------------------------------------------------------------------------------------------------
        // Successful context new block
        const char *memContextTestName = "test-new-block";
        MemContext *memContext = NULL;

        MEM_CONTEXT_NEW_BEGIN(memContextTestName)
        {
            memContext = MEM_CONTEXT_NEW();
            TEST_RESULT_PTR(memContext, memContextCurrent(), "new mem context is current");
            TEST_RESULT_STR(memContextName(memContext), memContextTestName, "context is now '%s'", memContextTestName);
        }
        MEM_CONTEXT_NEW_END();

        TEST_RESULT_PTR(memContextCurrent(), memContextTop(), "context is now 'TOP'");
        TEST_RESULT_BOOL(memContext->state == memContextStateActive, true, "new mem context is still active");
        memContextFree(memContext);

        // ------------------------------------------------------------------------------------------------------------------------
        // Failed context new block
        memContextTestName = "test-new-failed-block";
        bool bCatch = false;

        TRY_BEGIN()
        {
            MEM_CONTEXT_NEW_BEGIN(memContextTestName)
            {
                memContext = MEM_CONTEXT_NEW();
                TEST_RESULT_STR(memContextName(memContext), memContextTestName, "context is now '%s'", memContextTestName);
                THROW(AssertError, "create failed");
            }
            MEM_CONTEXT_NEW_END();
        }
        CATCH(AssertError)
        {
            bCatch = true;
        }
        TRY_END();

        TEST_RESULT_BOOL(bCatch, true, "new context error was caught");
        TEST_RESULT_PTR(memContextCurrent(), memContextTop(), "context is now 'TOP'");
        TEST_RESULT_UINT(memContext->state == memContextStateFree, true, "new mem context is not active");
    }

    // *****************************************************************************************************************************
    if (testBegin("memContextMove()"))
    {
        TEST_RESULT_VOID(memContextMove(NULL, memContextTop()), "move NULL context");

        // -------------------------------------------------------------------------------------------------------------------------
        MemContext *memContext = NULL;
        MemContext *memContext2 = NULL;
        void *mem = NULL;
        void *mem2 = NULL;

        MEM_CONTEXT_NEW_BEGIN("outer")
        {
            MEM_CONTEXT_TEMP_BEGIN()
            {
                memContextNew("not-to-be-moved");

                MEM_CONTEXT_NEW_BEGIN("inner")
                {
                    memContext = MEM_CONTEXT_NEW();
                    mem = memNew(sizeof(int));
                }
                MEM_CONTEXT_NEW_END();

                TEST_RESULT_PTR(memContext->allocList[0].buffer, mem, "check memory allocation");
                TEST_RESULT_PTR(memContextCurrent()->contextChildList[1], memContext, "check memory context");

                // Null out the mem context in the parent so the move will fail
                memContextCurrent()->contextChildList[1] = NULL;
                TEST_ERROR(memContextMove(memContext, MEM_CONTEXT_OLD()), AssertError, "unable to find mem context in old parent");

                // Set it back so the move will succeed
                memContextCurrent()->contextChildList[1] = memContext;
                TEST_RESULT_VOID(memContextMove(memContext, MEM_CONTEXT_OLD()), "move context");
                TEST_RESULT_VOID(memContextMove(memContext, MEM_CONTEXT_OLD()), "move context again");

                // Move another context
                MEM_CONTEXT_NEW_BEGIN("inner2")
                {
                    memContext2 = MEM_CONTEXT_NEW();
                    mem2 = memNew(sizeof(int));
                }
                MEM_CONTEXT_NEW_END();

                TEST_RESULT_VOID(memContextMove(memContext2, MEM_CONTEXT_OLD()), "move context");
            }
            MEM_CONTEXT_TEMP_END();

            TEST_RESULT_PTR(memContext->allocList[0].buffer, mem, "check memory allocation");
            TEST_RESULT_PTR(memContextCurrent()->contextChildList[1], memContext, "check memory context");

            TEST_RESULT_PTR(memContext2->allocList[0].buffer, mem2, "check memory allocation 2");
            TEST_RESULT_PTR(memContextCurrent()->contextChildList[2], memContext2, "check memory context 2");
        }
        MEM_CONTEXT_NEW_END();
    }

    memContextFree(memContextTop());

    FUNCTION_HARNESS_RESULT_VOID();
}
