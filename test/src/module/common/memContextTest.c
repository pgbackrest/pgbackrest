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
        // Make sure top context was created
        TEST_RESULT_Z(memContextName(memContextTop()), "TOP", "top context should exist");
        TEST_RESULT_INT(memContextTop()->contextChildListSize, 0, "top context should init with zero children");
        TEST_RESULT_PTR(memContextTop()->contextChildList, NULL, "top context child list empty");

        // Current context should equal top context
        TEST_RESULT_PTR(memContextCurrent(), memContextTop(), "top context == current context");

        // Context name length errors
        TEST_ERROR(memContextNew(""), AssertError, "assertion 'name[0] != '\\0'' failed");

        MemContext *memContext = memContextNew("test1");
        memContextKeep();
        TEST_RESULT_Z(memContextName(memContext), "test1", "test1 context name");
        TEST_RESULT_PTR(memContext->contextParent, memContextTop(), "test1 context parent is top");
        TEST_RESULT_INT(memContextTop()->contextChildListSize, MEM_CONTEXT_INITIAL_SIZE, "initial top context child list size");

        memContextSwitch(memContext);

        TEST_ERROR(memContextKeep(), AssertError, "new context expected but current context 'test1' found");
        TEST_ERROR(memContextDiscard(), AssertError, "new context expected but current context 'test1' found");
        TEST_RESULT_PTR(memContext, memContextCurrent(), "current context is now test1");

        // Create enough mem contexts to use up the initially allocated block
        for (int contextIdx = 1; contextIdx < MEM_CONTEXT_INITIAL_SIZE; contextIdx++)
        {
            memContextSwitch(memContextTop());
            memContextNew("test-filler");
            memContextKeep();
            TEST_RESULT_BOOL(
                memContextTop()->contextChildList[contextIdx]->state == memContextStateActive, true, "new context is active");
            TEST_RESULT_Z(memContextName(memContextTop()->contextChildList[contextIdx]), "test-filler", "new context name");
        }

        // This forces the child context array to grow
        memContextNew("test5");
        memContextKeep();
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
        memContextKeep();
        TEST_RESULT_BOOL(
            memContextTop()->contextChildList[1]->state == memContextStateActive,
            true, "new context in same index as freed context is active");
        TEST_RESULT_Z(memContextName(memContextTop()->contextChildList[1]), "test-reuse", "new context name");
        TEST_RESULT_UINT(memContextTop()->contextChildFreeIdx, 2, "check context free idx");

        // Next context will be at the end
        memContextNew("test-at-end");
        memContextKeep();
        TEST_RESULT_UINT(memContextTop()->contextChildFreeIdx, MEM_CONTEXT_INITIAL_SIZE + 2, "check context free idx");

        // Create a child context to test recursive free
        memContextSwitch(memContextTop()->contextChildList[MEM_CONTEXT_INITIAL_SIZE]);
        memContextNew("test-reuse");
        memContextKeep();
        TEST_RESULT_PTR_NE(
            memContextTop()->contextChildList[MEM_CONTEXT_INITIAL_SIZE]->contextChildList, NULL, "context child list is allocated");
        TEST_RESULT_INT(
            memContextTop()->contextChildList[MEM_CONTEXT_INITIAL_SIZE]->contextChildListSize, MEM_CONTEXT_INITIAL_SIZE,
            "context child list initial size");

        // This test will change if the contexts above change
        TEST_RESULT_UINT(memContextSize(memContextTop()), TEST_64BIT() ? 960 : 544, "check size");

        TEST_ERROR(
            memContextFree(memContextTop()->contextChildList[MEM_CONTEXT_INITIAL_SIZE]),
            AssertError, "cannot free current context 'test5'");

        memContextSwitch(memContextTop());
        memContextFree(memContextTop()->contextChildList[MEM_CONTEXT_INITIAL_SIZE]);

        TEST_ERROR(
            memContextFree(memContextTop()->contextChildList[MEM_CONTEXT_INITIAL_SIZE]),
            AssertError, "cannot free inactive context");

        MemContext *noAllocation = memContextNew("empty");
        memContextKeep();
        noAllocation->allocListSize = 0;
        free(noAllocation->allocList);
        TEST_RESULT_VOID(memContextFree(noAllocation), "free context with no allocations");
    }

    // *****************************************************************************************************************************
    if (testBegin("memContextAlloc(), memNew*(), memGrow(), and memFree()"))
    {
        TEST_RESULT_UINT(sizeof(MemContextAlloc), 8, "check MemContextAlloc size (same for 32/64 bit)");
        TEST_RESULT_PTR(MEM_CONTEXT_ALLOC_BUFFER((void *)1), (void *)(sizeof(MemContextAlloc) + 1), "check buffer macro");
        TEST_RESULT_PTR(MEM_CONTEXT_ALLOC_HEADER((void *)sizeof(MemContextAlloc)), (void *)0, "check header macro");

        memContextSwitch(memContextTop());
        memNewPtrArray(1);

        MemContext *memContext = memContextNew("test-alloc");
        TEST_ERROR(memContextSwitchBack(), AssertError, "current context expected but new context 'test-alloc' found");
        memContextKeep();
        memContextSwitch(memContext);

        for (int allocIdx = 0; allocIdx <= MEM_CONTEXT_ALLOC_INITIAL_SIZE; allocIdx++)
        {
            memNew(sizeof(size_t));

            TEST_RESULT_INT(
                memContextCurrent()->allocListSize,
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
        TEST_RESULT_UINT(memContextCurrent()->allocFreeIdx, MEM_CONTEXT_ALLOC_INITIAL_SIZE + 2, "check alloc free idx");
        TEST_RESULT_VOID(memFree(MEM_CONTEXT_ALLOC_BUFFER(memContextCurrent()->allocList[0])), "free allocation");
        TEST_RESULT_UINT(memContextCurrent()->allocFreeIdx, 0, "check alloc free idx");

        TEST_RESULT_VOID(memFree(MEM_CONTEXT_ALLOC_BUFFER(memContextCurrent()->allocList[1])), "free allocation");
        TEST_RESULT_UINT(memContextCurrent()->allocFreeIdx, 0, "check alloc free idx");

        TEST_RESULT_VOID(memNew(3), "new allocation");
        TEST_RESULT_UINT(memContextCurrent()->allocFreeIdx, 1, "check alloc free idx");

        TEST_RESULT_VOID(memNew(3), "new allocation");
        TEST_RESULT_UINT(memContextCurrent()->allocFreeIdx, 2, "check alloc free idx");

        TEST_RESULT_VOID(memNew(3), "new allocation");
        TEST_RESULT_UINT(memContextCurrent()->allocFreeIdx, MEM_CONTEXT_ALLOC_INITIAL_SIZE + 3, "check alloc free idx");

        // This test will change if the allocations above change
        TEST_RESULT_UINT(memContextSize(memContextCurrent()), TEST_64BIT() ? 249 : 165, "check size");

        TEST_ERROR(
            memFree(NULL), AssertError,
            "assertion '((MemContextAlloc *)buffer - 1) != NULL"
                " && (uintptr_t)((MemContextAlloc *)buffer - 1) != (uintptr_t)-sizeof(MemContextAlloc)"
                " && ((MemContextAlloc *)buffer - 1)->allocIdx <"
                " memContextStack[memContextCurrentStackIdx].memContext->allocListSize"
                " && memContextStack[memContextCurrentStackIdx].memContext->allocList[((MemContextAlloc *)buffer - 1)->allocIdx]'"
                " failed");
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
        memContext = memContextNew("test-callback-error");
        TEST_RESULT_VOID(memContextKeep(), "keep mem context");
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
            TEST_RESULT_PTR(MEM_CONTEXT_TEMP()->allocList[0], NULL, "nothing allocated");
            memNew(99);
            TEST_RESULT_PTR_NE(MEM_CONTEXT_TEMP()->allocList[0], NULL, "1 allocation");

            MEM_CONTEXT_TEMP_RESET(1);
            TEST_RESULT_PTR(MEM_CONTEXT_TEMP()->allocList[0], NULL, "nothing allocated");
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

        MEM_CONTEXT_NEW_BEGIN(memContextTestName)
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
            MEM_CONTEXT_NEW_BEGIN(memContextTestName)
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
                memContextKeep();

                MEM_CONTEXT_NEW_BEGIN("inner")
                {
                    memContext = MEM_CONTEXT_NEW();
                    mem = memNew(sizeof(int));
                }
                MEM_CONTEXT_NEW_END();

                TEST_RESULT_PTR(MEM_CONTEXT_ALLOC_BUFFER(memContext->allocList[0]), mem, "check memory allocation");
                TEST_RESULT_PTR(memContextCurrent()->contextChildList[1], memContext, "check memory context");

                // Null out the mem context in the parent so the move will fail
                memContextCurrent()->contextChildList[1] = NULL;
                TEST_ERROR(memContextMove(memContext, memContextPrior()), AssertError, "unable to find mem context in old parent");

                // Set it back so the move will succeed
                memContextCurrent()->contextChildList[1] = memContext;
                TEST_RESULT_VOID(memContextMove(memContext, memContextPrior()), "move context");
                TEST_RESULT_VOID(memContextMove(memContext, memContextPrior()), "move context again");

                // Move another context
                MEM_CONTEXT_NEW_BEGIN("inner2")
                {
                    memContext2 = MEM_CONTEXT_NEW();
                    mem2 = memNew(sizeof(int));
                }
                MEM_CONTEXT_NEW_END();

                TEST_RESULT_VOID(memContextMove(memContext2, memContextPrior()), "move context");
            }
            MEM_CONTEXT_TEMP_END();

            TEST_RESULT_PTR(MEM_CONTEXT_ALLOC_BUFFER(memContext->allocList[0]), mem, "check memory allocation");
            TEST_RESULT_PTR(memContextCurrent()->contextChildList[1], memContext, "check memory context");

            TEST_RESULT_PTR(MEM_CONTEXT_ALLOC_BUFFER(memContext2->allocList[0]), mem2, "check memory allocation 2");
            TEST_RESULT_PTR(memContextCurrent()->contextChildList[2], memContext2, "check memory context 2");
        }
        MEM_CONTEXT_NEW_END();
    }

    memContextFree(memContextTop());

    FUNCTION_HARNESS_RETURN_VOID();
}
