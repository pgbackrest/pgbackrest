/***********************************************************************************************************************************
Test Memory Contexts
***********************************************************************************************************************************/

/***********************************************************************************************************************************
testFree - test callback function
***********************************************************************************************************************************/
MemContext *memContextCallbackArgument = NULL;

void
testFree(MemContext *this)
{
    TEST_RESULT_INT(this->state, memContextStateFreeing, "state should be freeing before memContextFree() in callback");
    memContextFree(this);
    TEST_RESULT_INT(this->state, memContextStateFreeing, "state should still be freeing after memContextFree() in callback");

    TEST_ERROR(
        memContextCallback(this, (MemContextCallback)testFree, this),
        AssertError, "cannot assign callback to inactive context");

    TEST_ERROR(memContextSwitch(this), AssertError, "cannot switch to inactive context");
    TEST_ERROR(memContextName(this), AssertError, "cannot get name for inactive context");

    memContextCallbackArgument = this;
}

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun()
{
    // -----------------------------------------------------------------------------------------------------------------------------
    if (testBegin("memAllocInternal(), memReAllocInternal(), and memFreeInternal()"))
    {
        // Test too large allocation -- only test this on 64-bit systems since 32-bit systems tend to work with any value that
        // valgrind will accept
        if (sizeof(size_t) == 8)
        {
            TEST_ERROR(memAllocInternal((size_t)5629499534213120, false), MemoryError, "unable to allocate 5629499534213120 bytes");
            TEST_ERROR(memFreeInternal(NULL), MemoryError, "unable to free null pointer");

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
            if (buffer2[charIdx] == 0)
                expectedTotal++;

        TEST_RESULT_INT(expectedTotal, sizeof(size_t), "all bytes are 0");

        // Zeroed memory reallocation
        memset(buffer2, 0xC7, sizeof(size_t));

        buffer2 = memReAllocInternal(buffer2, sizeof(size_t), sizeof(size_t) * 2, true);

        expectedTotal = 0;

        for (unsigned int charIdx = 0; charIdx < sizeof(size_t); charIdx++)
            if (buffer2[charIdx] == 0xC7)
                expectedTotal++;

        TEST_RESULT_INT(expectedTotal, sizeof(size_t), "all old bytes are filled");

        expectedTotal = 0;

        for (unsigned int charIdx = 0; charIdx < sizeof(size_t); charIdx++)
            if ((buffer2 + sizeof(size_t))[charIdx] == 0)
                expectedTotal++;

        TEST_RESULT_INT(expectedTotal, sizeof(size_t), "all new bytes are 0");
        memFreeInternal(buffer2);
    }

    // -----------------------------------------------------------------------------------------------------------------------------
    if (testBegin("memContextNew() and memContextFree()"))
    {
        // Make sure top context was created
        TEST_RESULT_STR(memContextName(memContextTop()), "TOP", "top context should exist");
        TEST_RESULT_INT(memContextTop()->contextChildListSize, 0, "top context should init with zero children");
        TEST_RESULT_PTR(memContextTop()->contextChildList, NULL, "top context child list empty");

        // TEST_ERROR(memContextFree(memContextTop()), AssertError, "cannot free top context");

        // Current context should equal top context
        TEST_RESULT_PTR(memContextCurrent(), memContextTop(), "top context == current context");

        // Context name length errors
        TEST_ERROR(memContextNew(""), AssertError, "context name length must be > 0 and <= 64");
        TEST_ERROR(
            memContextNew("12345678901234567890123456789012345678901234567890123456789012345"),
            AssertError, "context name length must be > 0 and <= 64");

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

        // Free a context
        memContextFree(memContextTop()->contextChildList[1]);
        TEST_RESULT_BOOL(
            memContextTop()->contextChildList[1]->state == memContextStateActive, false, "child context inactive after free");
        TEST_RESULT_PTR(memContextCurrent(), memContextTop(), "current context is top");

        // Create a new context and it should end up in the same spot
        memContextNew("test-reuse");
        TEST_RESULT_BOOL(
            memContextTop()->contextChildList[1]->state == memContextStateActive,
            true, "new context in same index as freed context is active");
        TEST_RESULT_STR(memContextName(memContextTop()->contextChildList[1]), "test-reuse", "new context name");

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
    }

    // -----------------------------------------------------------------------------------------------------------------------------
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
                if (buffer[charIdx] == 0)
                    expectedTotal++;

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
            if (buffer[charIdx] == 0xFE)
                expectedTotal++;

        TEST_RESULT_INT(expectedTotal, sizeof(size_t), "all bytes are 0xFE in original portion");

        // Free memory
        TEST_ERROR(memFree(NULL), AssertError, "unable to find null allocation");
        TEST_ERROR(memFree((void *)0x01), AssertError, "unable to find allocation");
        memFree(buffer);

        memContextSwitch(memContextTop());
        memContextFree(memContext);
    }

    // -----------------------------------------------------------------------------------------------------------------------------
    if (testBegin("memContextCallback()"))
    {
        TEST_ERROR(memContextCallback(memContextTop(), NULL, NULL), AssertError, "top context may not have a callback");

        MemContext *memContext = memContextNew("test-callback");
        memContextCallback(memContext, (MemContextCallback)testFree, memContext);
        TEST_ERROR(
            memContextCallback(memContext, (MemContextCallback)testFree, memContext),
            AssertError, "callback is already set for context 'test-callback'");

        memContextFree(memContext);
        TEST_RESULT_PTR(memContextCallbackArgument, memContext, "callback argument is context");
    }

    // -----------------------------------------------------------------------------------------------------------------------------
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

    // -----------------------------------------------------------------------------------------------------------------------------
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
        TEST_RESULT_BOOL(memContext->state == memContextStateFree, true, "new mem context is not active");
    }

    memContextFree(memContextTop());
}
