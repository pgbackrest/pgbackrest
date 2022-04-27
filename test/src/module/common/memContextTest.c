/***********************************************************************************************************************************
Test Memory Contexts
***********************************************************************************************************************************/

/***********************************************************************************************************************************
testFree - test callback function
***********************************************************************************************************************************/
static MemContext *memContextCallbackArgument = NULL;
static bool testFreeThrow = false;

static void
testFree(void *thisVoid)
{
    MemContext *this = thisVoid;

    TEST_RESULT_BOOL(memContextFreeing(this), true, "state should be freeing before memContextFree() in callback");
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
static void
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
            TEST_ERROR(memAllocInternal((size_t)5629499534213120), MemoryError, "unable to allocate 5629499534213120 bytes");
            TEST_ERROR(memFreeInternal(NULL), AssertError, "assertion 'buffer != NULL' failed");

            // Check that bad realloc is caught
            void *buffer = memAllocInternal(sizeof(size_t));
            TEST_ERROR(
                memReAllocInternal(buffer, (size_t)5629499534213120), MemoryError,
                "unable to reallocate 5629499534213120 bytes");
            memFreeInternal(buffer);
        }

        // Memory allocation
        void *buffer = memAllocInternal(sizeof(size_t));

        // Memory reallocation
        memset(buffer, 0xC7, sizeof(size_t));

        unsigned char *buffer2 = memReAllocInternal(buffer, sizeof(size_t) * 2);

        int expectedTotal = 0;

        for (unsigned int charIdx = 0; charIdx < sizeof(size_t); charIdx++)
            expectedTotal += buffer2[charIdx] == 0xC7;

        TEST_RESULT_INT(expectedTotal, sizeof(size_t), "all old bytes are filled");
        memFreeInternal(buffer2);
    }

    // *****************************************************************************************************************************
    if (testBegin("memContextNew() and memContextFree()"))
    {
        TEST_TITLE("struct size");

        TEST_RESULT_UINT(sizeof(MemContext), TEST_64BIT() ? 24 : 16, "MemContext size");
        TEST_RESULT_UINT(sizeof(MemContextChildMany), TEST_64BIT() ? 16 : 12, "MemContextChildMany size");
        TEST_RESULT_UINT(sizeof(MemContextAllocMany), TEST_64BIT() ? 16 : 12, "MemContextAllocMany size");
        TEST_RESULT_UINT(sizeof(MemContextCallback), TEST_64BIT() ? 16 : 8, "MemContextCallback size");

        // -------------------------------------------------------------------------------------------------------------------------
        // Make sure top context was created
        TEST_RESULT_Z(memContextName(memContextTop()), "TOP", "top context should exist");
        TEST_RESULT_BOOL(memContextTop()->childInitialized, false, "top context should init with zero children");

        // Current context should equal top context
        TEST_RESULT_PTR(memContextCurrent(), memContextTop(), "top context == current context");

        // Context name length errors
        TEST_ERROR(memContextNewP(""), AssertError, "assertion 'name[0] != '\\0'' failed");

        MemContext *memContext = memContextNewP("test1", .childType = memContextChildTypeMany, .allocType = memContextAllocTypeMany, .callback = true);
        memContextKeep();
        TEST_RESULT_Z(memContextName(memContext), "test1", "test1 context name");
        TEST_RESULT_PTR(memContext->contextParent, memContextTop(), "test1 context parent is top");
        TEST_RESULT_INT(
            memContextChildMany(memContextTop())->listSize, MEM_CONTEXT_INITIAL_SIZE, "initial top context child list size");

        memContextSwitch(memContext);

        TEST_ERROR(memContextKeep(), AssertError, "new context expected but current context 'test1' found");
        TEST_ERROR(memContextDiscard(), AssertError, "new context expected but current context 'test1' found");
        TEST_RESULT_PTR(memContext, memContextCurrent(), "current context is now test1");

        // Create enough mem contexts to use up the initially allocated block
        memContextSwitch(memContextTop());

        for (int contextIdx = 1; contextIdx < MEM_CONTEXT_INITIAL_SIZE; contextIdx++)
        {
            memContextNewP("test-filler", .childType = memContextChildTypeMany, .allocType = memContextAllocTypeMany, .callback = true);
            memContextKeep();
            TEST_RESULT_PTR_NE(
                memContextChildMany(memContextTop())->list[contextIdx], NULL,
                "new context exists");
            TEST_RESULT_BOOL(
                memContextChildMany(memContextTop())->list[contextIdx]->state == memContextStateActive, true,
                "new context is active");
            TEST_RESULT_Z(
                memContextName(memContextChildMany(memContextTop())->list[contextIdx]), "test-filler", "new context name");
        }

        // This forces the child context array to grow
        memContext = memContextNewP("test5", .allocExtra = 16, .childType = memContextChildTypeMany, .allocType = memContextAllocTypeMany, .callback = true);
        TEST_RESULT_PTR(memContextAllocExtra(memContext), memContext + 1, "mem context alloc extra");
        TEST_RESULT_PTR(memContextFromAllocExtra(memContext + 1), memContext, "mem context from alloc extra");
        TEST_RESULT_PTR(memContextConstFromAllocExtra(memContext + 1), memContext, "const mem context from alloc extra");
        memContextKeep();
        TEST_RESULT_INT(
            memContextChildMany(memContextTop())->listSize, MEM_CONTEXT_INITIAL_SIZE * 2, "increased child context list size");
        TEST_RESULT_UINT(memContextChildMany(memContextTop())->freeIdx, MEM_CONTEXT_INITIAL_SIZE + 1, "check context free idx");

        // Free a context
        memContextFree(memContextChildMany(memContextTop())->list[1]);
        TEST_RESULT_PTR(memContextChildMany(memContextTop())->list[1], NULL, "child context NULL after free");
        TEST_RESULT_PTR(memContextCurrent(), memContextTop(), "current context is top");
        TEST_RESULT_UINT(memContextChildMany(memContextTop())->freeIdx, 1, "check context free idx");

        // Create a new context and it should end up in the same spot
        memContextNewP("test-reuse", .childType = memContextChildTypeMany, .allocType = memContextAllocTypeMany, .callback = true);
        memContextKeep();
        TEST_RESULT_BOOL(
            memContextChildMany(memContextTop())->list[1]->state == memContextStateActive,
            true, "new context in same index as freed context is active");
        TEST_RESULT_Z(memContextName(memContextChildMany(memContextTop())->list[1]), "test-reuse", "new context name");
        TEST_RESULT_UINT(memContextChildMany(memContextTop())->freeIdx, 2, "check context free idx");

        // Next context will be at the end
        memContextNewP("test-at-end", .childType = memContextChildTypeMany, .allocType = memContextAllocTypeMany, .callback = true);
        memContextKeep();
        TEST_RESULT_UINT(memContextChildMany(memContextTop())->freeIdx, MEM_CONTEXT_INITIAL_SIZE + 2, "check context free idx");

        // Create a child context to test recursive free
        memContextSwitch(memContextChildMany(memContextTop())->list[MEM_CONTEXT_INITIAL_SIZE]);
        memContextNewP("test-reuse", .childType = memContextChildTypeMany, .allocType = memContextAllocTypeMany, .callback = true);
        memContextKeep();
        TEST_RESULT_PTR_NE(
            memContextChildMany(memContextChildMany(memContextTop())->list[MEM_CONTEXT_INITIAL_SIZE])->list, NULL,
            "context child list is allocated");
        TEST_RESULT_INT(
            memContextChildMany(memContextChildMany(memContextTop())->list[MEM_CONTEXT_INITIAL_SIZE])->listSize,
            MEM_CONTEXT_INITIAL_SIZE, "context child list initial size");

        // This test will change if the contexts above change
        TEST_RESULT_UINT(memContextSize(memContextTop()), TEST_64BIT() ? 672 : 440, "check size");

        TEST_ERROR(
            memContextFree(memContextChildMany(memContextTop())->list[MEM_CONTEXT_INITIAL_SIZE]), AssertError,
            "cannot free current context 'test5'");

        memContextSwitch(memContextTop());
        // memContextFree(memContextTop()->contextChildList[MEM_CONTEXT_INITIAL_SIZE]);
        memContextFree(memContextTop());

        MemContext *noAllocation = memContextNewP("empty", .childType = memContextChildTypeMany, .allocType = memContextAllocTypeMany, .callback = true);
        memContextKeep();
        // memContextAllocMany(noAllocation)->listSize = 0; !!! NO LONGER NEEDED?
        // free(memContextAllocMany(noAllocation->list)); !!! NO LONGER NEEDED?
        TEST_RESULT_VOID(memContextFree(noAllocation), "free context with no allocations");
    }

    // *****************************************************************************************************************************
    if (testBegin("memContextAlloc*(), memNew*(), memGrow(), and memFree()"))
    {
        TEST_TITLE("struct size");

        TEST_RESULT_UINT(sizeof(MemContextAlloc), 8, "MemContextAlloc size");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_UINT(sizeof(MemContextAlloc), 8, "check MemContextAlloc size (same for 32/64 bit)");
        TEST_RESULT_PTR(MEM_CONTEXT_ALLOC_BUFFER((void *)1), (void *)(sizeof(MemContextAlloc) + 1), "check buffer macro");
        TEST_RESULT_PTR(MEM_CONTEXT_ALLOC_HEADER((void *)sizeof(MemContextAlloc)), (void *)0, "check header macro");

        memContextSwitch(memContextTop());
        memNewPtrArray(1);

        MemContext *memContext = memContextNewP("test-alloc", .childType = memContextChildTypeMany, .allocType = memContextAllocTypeMany, .callback = true);
        TEST_ERROR(memContextSwitchBack(), AssertError, "current context expected but new context 'test-alloc' found");
        memContextKeep();
        memContextSwitch(memContext);

        for (int allocIdx = 0; allocIdx <= MEM_CONTEXT_ALLOC_INITIAL_SIZE; allocIdx++)
        {
            memNew(sizeof(size_t));

            TEST_RESULT_INT(
                memContextAllocMany(memContextCurrent())->listSize,
                allocIdx == MEM_CONTEXT_ALLOC_INITIAL_SIZE ? MEM_CONTEXT_ALLOC_INITIAL_SIZE * 2 : MEM_CONTEXT_ALLOC_INITIAL_SIZE,
                "allocation list size");
        }

        unsigned char *buffer = memNew(sizeof(size_t));

        // Grow memory
        memset(buffer, 0xFE, sizeof(size_t));
        buffer = memResize(buffer, sizeof(size_t) * 2);

        // Check that original portion of the buffer is preserved
        int expectedTotal = 0;

        for (unsigned int charIdx = 0; charIdx < sizeof(size_t); charIdx++)
            expectedTotal += buffer[charIdx] == 0xFE;

        TEST_RESULT_INT(expectedTotal, sizeof(size_t), "all bytes are 0xFE in original portion");

        // Free memory
        TEST_RESULT_UINT(
            memContextAllocMany(memContextCurrent())->freeIdx, MEM_CONTEXT_ALLOC_INITIAL_SIZE + 2, "check alloc free idx");
        TEST_RESULT_VOID(memFree(MEM_CONTEXT_ALLOC_BUFFER(memContextAllocMany(memContextCurrent())->list[0])), "free allocation");
        TEST_RESULT_UINT(memContextAllocMany(memContextCurrent())->freeIdx, 0, "check alloc free idx");

        TEST_RESULT_VOID(memFree(MEM_CONTEXT_ALLOC_BUFFER(memContextAllocMany(memContextCurrent())->list[1])), "free allocation");
        TEST_RESULT_UINT(memContextAllocMany(memContextCurrent())->freeIdx, 0, "check alloc free idx");

        TEST_RESULT_VOID(memNew(3), "new allocation");
        TEST_RESULT_UINT(memContextAllocMany(memContextCurrent())->freeIdx, 1, "check alloc free idx");

        TEST_RESULT_VOID(memNew(3), "new allocation");
        TEST_RESULT_UINT(memContextAllocMany(memContextCurrent())->freeIdx, 2, "check alloc free idx");

        TEST_RESULT_VOID(memNew(3), "new allocation");
        TEST_RESULT_UINT(
            memContextAllocMany(memContextCurrent())->freeIdx, MEM_CONTEXT_ALLOC_INITIAL_SIZE + 3, "check alloc free idx");

        // This test will change if the allocations above change
        // TEST_RESULT_UINT(memContextSize(memContextCurrent()), TEST_64BIT() ? 241 : 165, "check size");

        // TEST_ERROR(
        //     memFree(NULL), AssertError,
        //     "assertion '((MemContextAlloc *)buffer - 1) != NULL"
        //         " && (uintptr_t)((MemContextAlloc *)buffer - 1) != (uintptr_t)-sizeof(MemContextAlloc)"
        //         " && ((MemContextAlloc *)buffer - 1)->allocIdx <"
        //         " memContextStack[memContextCurrentStackIdx].memContext->allocListSize"
        //         " && memContextStack[memContextCurrentStackIdx].memContext->allocList[((MemContextAlloc *)buffer - 1)->allocIdx]'"
        //         " failed");
        memFree(buffer);

        memContextSwitch(memContextTop());
        memContextFree(memContext);
    }

    // *****************************************************************************************************************************
    if (testBegin("memContextCallbackSet()"))
    {
        TEST_ERROR(
            memContextCallbackSet(memContextTop(), testFree, NULL), AssertError, "assertion 'this->callback' failed");

        MemContext *memContext = memContextNewP("test-callback", .childType = memContextChildTypeMany, .allocType = memContextAllocTypeMany, .callback = true);
        memContextKeep();
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
        memContext = memContextNewP("test-callback-error", .childType = memContextChildTypeMany, .allocType = memContextAllocTypeMany, .callback = true);
        TEST_RESULT_VOID(memContextKeep(), "keep mem context");
        testFreeThrow = true;
        TEST_RESULT_VOID(memContextCallbackSet(memContext, testFree, memContext), "    set callback");
        TEST_ERROR(memContextFree(memContext), AssertError, "error in callback");
    }

    // *****************************************************************************************************************************
    if (testBegin("MEM_CONTEXT_BEGIN() and MEM_CONTEXT_END()"))
    {
        memContextSwitch(memContextTop());
        MemContext *memContext = memContextNewP("test-block", .childType = memContextChildTypeMany, .allocType = memContextAllocTypeMany, .callback = true);
        memContextKeep();

        // Check normal block
        MEM_CONTEXT_BEGIN(memContext)
        {
            TEST_RESULT_Z(memContextName(memContextCurrent()), "test-block", "context is now test-block");
        }
        MEM_CONTEXT_END();

        TEST_RESULT_Z(memContextName(memContextCurrent()), "TOP", "context is now top");

        // Check block that errors
        TEST_ERROR(
            MEM_CONTEXT_BEGIN(memContext)
            {
                TEST_RESULT_Z(memContextName(memContextCurrent()), "test-block", "context is now test-block");
                THROW(AssertError, "error in test block");
            }
            MEM_CONTEXT_END(),
            AssertError, "error in test block");

        TEST_RESULT_Z(memContextName(memContextCurrent()), "TOP", "context is now top");

        // Reset temp mem context after a single interaction
        // -------------------------------------------------------------------------------------------------------------------------
        MEM_CONTEXT_TEMP_RESET_BEGIN()
        {
            TEST_RESULT_BOOL(MEM_CONTEXT_TEMP()->allocInitialized, false, "nothing allocated");
            memNew(99);
            TEST_RESULT_PTR_NE(memContextAllocMany(MEM_CONTEXT_TEMP())->list[0], NULL, "1 allocation");

            MEM_CONTEXT_TEMP_RESET(1);
            TEST_RESULT_BOOL(MEM_CONTEXT_TEMP()->allocInitialized, false, "nothing allocated");
        }
        MEM_CONTEXT_TEMP_END();
    }

    // *****************************************************************************************************************************
    if (testBegin("MEM_CONTEXT_NEW_BEGIN() and MEM_CONTEXT_NEW_END()"))
    {
        // ------------------------------------------------------------------------------------------------------------------------
        // Successful context new block
        const char *memContextTestName = "test-new-block";
        MemContext *memContext = NULL;

        TEST_RESULT_PTR(memContextCurrent(), memContextTop(), "context is now 'TOP'");

        MEM_CONTEXT_NEW_BEGIN(memContextTestName, .childType = memContextChildTypeMany, .allocType = memContextAllocTypeMany)
        {
            memContext = MEM_CONTEXT_NEW();
            TEST_RESULT_PTR(memContext, memContextCurrent(), "new mem context is current");
            TEST_RESULT_Z(memContextName(memContext), memContextTestName, "check context name");
        }
        MEM_CONTEXT_NEW_END();

        TEST_RESULT_Z(memContextName(memContextCurrent()), "TOP", "context name is now 'TOP'");
        TEST_RESULT_PTR(memContextCurrent(), memContextTop(), "context is now 'TOP'");
        TEST_RESULT_BOOL(memContext->state == memContextStateActive, true, "new mem context is still active");
        memContextFree(memContext);

        // ------------------------------------------------------------------------------------------------------------------------
        // Failed context new block
        memContextTestName = "test-new-failed-block";
        bool bCatch = false;

        TRY_BEGIN()
        {
            MEM_CONTEXT_NEW_BEGIN(memContextTestName, .childType = memContextChildTypeMany, .allocType = memContextAllocTypeMany)
            {
                memContext = MEM_CONTEXT_NEW();
                TEST_RESULT_Z(memContextName(memContext), memContextTestName, "check mem context name");
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

        MEM_CONTEXT_NEW_BEGIN("outer", .childType = memContextChildTypeMany)
        {
            MEM_CONTEXT_TEMP_BEGIN()
            {
                memContextNewP("not-to-be-moved", .childType = memContextChildTypeMany);
                memContextKeep();

                MEM_CONTEXT_NEW_BEGIN("inner", .allocType = memContextAllocTypeMany)
                {
                    memContext = MEM_CONTEXT_NEW();
                    mem = memNew(sizeof(int));
                }
                MEM_CONTEXT_NEW_END();

                TEST_RESULT_PTR(MEM_CONTEXT_ALLOC_BUFFER(memContextAllocMany(memContext)->list[0]), mem, "check memory allocation");
                TEST_RESULT_PTR(memContextChildMany(memContextCurrent())->list[1], memContext, "check memory context");

                // Null out the mem context in the parent so the move will fail
                memContextChildMany(memContextCurrent())->list[1] = NULL;
                TEST_ERROR(
                    memContextMove(memContext, memContextPrior()), AssertError,
                    "assertion 'memContextChildMany(this->contextParent)->list[this->contextParentIdx] == this' failed");

                // Set it back so the move will succeed
                memContextChildMany(memContextCurrent())->list[1] = memContext;
                TEST_RESULT_VOID(memContextMove(memContext, memContextPrior()), "move context");
                TEST_RESULT_VOID(memContextMove(memContext, memContextPrior()), "move context again");

                // Move another context
                MEM_CONTEXT_NEW_BEGIN("inner2", .allocType = memContextAllocTypeMany)
                {
                    memContext2 = MEM_CONTEXT_NEW();
                    mem2 = memNew(sizeof(int));
                }
                MEM_CONTEXT_NEW_END();

                TEST_RESULT_VOID(memContextMove(memContext2, memContextPrior()), "move context");
            }
            MEM_CONTEXT_TEMP_END();

            TEST_RESULT_PTR(MEM_CONTEXT_ALLOC_BUFFER(memContextAllocMany(memContext)->list[0]), mem, "check memory allocation");
            TEST_RESULT_PTR(memContextChildMany(memContextCurrent())->list[1], memContext, "check memory context");

            TEST_RESULT_PTR(MEM_CONTEXT_ALLOC_BUFFER(memContextAllocMany(memContext2)->list[0]), mem2, "check memory allocation 2");
            TEST_RESULT_PTR(memContextChildMany(memContextCurrent())->list[2], memContext2, "check memory context 2");
        }
        MEM_CONTEXT_NEW_END();
    }

    memContextFree(memContextTop());

    FUNCTION_HARNESS_RETURN_VOID();
}
