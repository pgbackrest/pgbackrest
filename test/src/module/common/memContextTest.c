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

    TEST_RESULT_BOOL(this->active, false, "mem context inactive in callback");

    TEST_ERROR(memContextFree(this), AssertError, "assertion 'this->active' failed");
    TEST_ERROR(memContextCallbackSet(this, testFree, this), AssertError, "assertion 'this->active' failed");
    TEST_ERROR(memContextSwitch(this), AssertError, "assertion 'this->active' failed");

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

        TEST_RESULT_UINT(sizeof(MemContext), TEST_64BIT() ? 32 : 24, "MemContext size");
        TEST_RESULT_UINT(sizeof(MemContextChildMany), TEST_64BIT() ? 16 : 12, "MemContextChildMany size");
        TEST_RESULT_UINT(sizeof(MemContextAllocMany), TEST_64BIT() ? 16 : 12, "MemContextAllocMany size");
        TEST_RESULT_UINT(sizeof(MemContextCallbackOne), TEST_64BIT() ? 16 : 8, "MemContextCallbackOne size");

        // -------------------------------------------------------------------------------------------------------------------------
        // Make sure top context was created
        TEST_RESULT_Z(memContextTop()->name, "TOP", "top context should exist");
        TEST_RESULT_BOOL(memContextTop()->childInitialized, false, "top context should init with zero children");

        // Current context should equal top context
        TEST_RESULT_PTR(memContextCurrent(), memContextTop(), "top context == current context");

        // Context name length errors
        TEST_ERROR(memContextNewP(""), AssertError, "assertion 'name[0] != '\\0'' failed");

        MemContext *memContext = memContextNewP(
            "test1", .childQty = MEM_CONTEXT_QTY_MAX, .allocQty = MEM_CONTEXT_QTY_MAX, .callbackQty = 1);
        memContextKeep();
        TEST_RESULT_Z(memContext->name, "test1", "test1 context name");
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
            memContextNewP("test-filler", .childQty = MEM_CONTEXT_QTY_MAX, .allocQty = MEM_CONTEXT_QTY_MAX, .callbackQty = 1);
            memContextKeep();
            TEST_RESULT_PTR_NE(memContextChildMany(memContextTop())->list[contextIdx], NULL, "new context exists");
            TEST_RESULT_BOOL(memContextChildMany(memContextTop())->list[contextIdx]->active, true, "new context is active");
            TEST_RESULT_Z(memContextChildMany(memContextTop())->list[contextIdx]->name, "test-filler", "new context name");
        }

        // This forces the child context array to grow
        memContext = memContextNewP(
            "test5", .allocExtra = 16, .childQty = MEM_CONTEXT_QTY_MAX, .allocQty = MEM_CONTEXT_QTY_MAX, .callbackQty = 1);
        TEST_RESULT_PTR(memContextAllocExtra(memContext), memContext + 1, "mem context alloc extra");
        TEST_RESULT_PTR(memContextFromAllocExtra(memContext + 1), memContext, "mem context from alloc extra");
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
        MemContext *const memContextOneChild = memContextNewP("test-reuse", .childQty = 1);
        memContextKeep();
        TEST_RESULT_BOOL(
            memContextChildMany(memContextTop())->list[1]->active, true, "new context in same index as freed context is active");
        TEST_RESULT_Z(memContextChildMany(memContextTop())->list[1]->name, "test-reuse", "new context name");
        TEST_RESULT_UINT(memContextChildMany(memContextTop())->freeIdx, 2, "check context free idx");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("child context in context with only one child allowed");

        TEST_RESULT_VOID(memContextSwitch(memContextOneChild), "switch");
        MemContext *memContextSingleChild;
        TEST_ASSIGN(memContextSingleChild, memContextNewP("single-child", .childQty = 1, .allocQty = 1), "new");
        TEST_RESULT_VOID(memContextKeep(), "keep");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("child context freed in context with only one child allowed");

        TEST_RESULT_VOID(memContextSwitch(memContextSingleChild), "switch");
        TEST_RESULT_VOID(memContextNewP("free-child", .childQty = 1), "new");
        TEST_RESULT_VOID(memContextDiscard(), "discard");

        TEST_RESULT_VOID(memContextSwitch(memContextTop()), "switch to top");

        // -------------------------------------------------------------------------------------------------------------------------
        // Next context will be at the end
        memContextNewP("test-at-end");
        memContextKeep();
        TEST_RESULT_UINT(memContextChildMany(memContextTop())->freeIdx, MEM_CONTEXT_INITIAL_SIZE + 2, "check context free idx");

        // Create a child context to test recursive free
        memContextSwitch(memContextChildMany(memContextTop())->list[MEM_CONTEXT_INITIAL_SIZE]);
        memContextNewP("test-reuse", .childQty = 1);
        memContextKeep();
        TEST_RESULT_PTR_NE(
            memContextChildMany(memContextChildMany(memContextTop())->list[MEM_CONTEXT_INITIAL_SIZE])->list, NULL,
            "context child list is allocated");
        TEST_RESULT_INT(
            memContextChildMany(memContextChildMany(memContextTop())->list[MEM_CONTEXT_INITIAL_SIZE])->listSize,
            MEM_CONTEXT_INITIAL_SIZE, "context child list initial size");

        // This test will change if the contexts above change
        TEST_RESULT_UINT(memContextSize(memContextTop()), TEST_64BIT() ? 656 : 448, "check size");

        TEST_ERROR(
            memContextFree(memContextChildMany(memContextTop())->list[MEM_CONTEXT_INITIAL_SIZE]), AssertError,
            "cannot free current context 'test5'");

        TEST_RESULT_VOID(memContextSwitch(memContextTop()), "switch to top");
        TEST_RESULT_VOID(memContextFree(memContextTop()), "free top");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("alloc extra not aligned");

        TEST_ASSIGN(memContext, memContextNewP("test-alloc", .allocExtra = 7), "no aligned");
        TEST_RESULT_UINT(memContext->allocExtra, 8, "check");
        TEST_RESULT_VOID(memContextDiscard(), "discard");
    }

    // *****************************************************************************************************************************
    if (testBegin("memContextAlloc*(), memNew*(), memGrow(), and memFree()"))
    {
        TEST_TITLE("struct size");

        TEST_RESULT_UINT(sizeof(MemContextAlloc), 8, "check MemContextAlloc size (same for 32/64 bit)");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_PTR(MEM_CONTEXT_ALLOC_BUFFER((void *)1), (void *)(sizeof(MemContextAlloc) + 1), "check buffer macro");
        TEST_RESULT_PTR(MEM_CONTEXT_ALLOC_HEADER((void *)sizeof(MemContextAlloc)), (void *)0, "check header macro");

        memContextSwitch(memContextTop());
        memNewPtrArray(1);

        MemContext *memContext = memContextNewP("test-alloc", .allocQty = MEM_CONTEXT_QTY_MAX);
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
        TEST_RESULT_UINT(memContextSize(memContextCurrent()), TEST_64BIT() ? 217 : 153, "check size");

        TEST_ERROR(
            memFree(NULL), AssertError,
            "assertion 'alloc != NULL && "
            "(uintptr_t)alloc != (uintptr_t)-sizeof(MemContextAlloc) &&"
            " alloc->allocIdx < memContextAllocMany(memContextStack[memContextCurrentStackIdx].memContext)->listSize &&"
            " memContextAllocMany(memContextStack[memContextCurrentStackIdx].memContext)->list[alloc->allocIdx]' failed");
        memFree(buffer);

        memContextSwitch(memContextTop());
        memContextFree(memContext);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("mem context with one allocation");

        TEST_ASSIGN(memContext, memContextNewP("test-alloc", .allocQty = 1), "new");
        TEST_RESULT_VOID(memContextKeep(), "keep new");
        TEST_RESULT_VOID(memContextSwitch(memContext), "switch to new");

        TEST_ASSIGN(buffer, memNew(100), "new");
        TEST_ASSIGN(buffer, memResize(buffer, 150), "resize");
        TEST_RESULT_VOID(memFree(buffer), "free");

        TEST_ASSIGN(buffer, memNew(200), "new");

        // This test will change if the allocations above change
        TEST_RESULT_UINT(memContextSize(memContextCurrent()), TEST_64BIT() ? 248 : 236, "check size");

        TEST_RESULT_VOID(memContextSwitch(memContextTop()), "switch to top");
        TEST_RESULT_VOID(memContextFree(memContext), "context free");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("mem context with one allocation freed before context free");

        TEST_ASSIGN(memContext, memContextNewP("test-alloc", .allocQty = 1), "new");
        TEST_RESULT_VOID(memContextKeep(), "keep new");
        TEST_RESULT_VOID(memContextSwitch(memContext), "switch to new");

        TEST_ASSIGN(buffer, memNew(100), "new");
        TEST_RESULT_VOID(memFree(buffer), "free");

        // This test will change if the allocations above change
        TEST_RESULT_UINT(memContextSize(memContextCurrent()), TEST_64BIT() ? 40 : 28, "check size");

        TEST_RESULT_VOID(memContextSwitch(memContextTop()), "switch to top");
        TEST_RESULT_VOID(memContextFree(memContext), "context free");
    }

    // *****************************************************************************************************************************
    if (testBegin("memContextCallbackSet()"))
    {
        TEST_ERROR(
            memContextCallbackSet(memContextTop(), testFree, NULL), AssertError,
            "assertion 'this->callbackQty != memQtyNone' failed");

        MemContext *memContext = memContextNewP(
            "test-callback", .childQty = MEM_CONTEXT_QTY_MAX, .allocQty = MEM_CONTEXT_QTY_MAX, .callbackQty = 1);
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
        memContext = memContextNewP(
            "test-callback-error", .childQty = MEM_CONTEXT_QTY_MAX, .allocQty = MEM_CONTEXT_QTY_MAX, .callbackQty = 1);
        TEST_RESULT_VOID(memContextKeep(), "keep mem context");
        testFreeThrow = true;
        TEST_RESULT_VOID(memContextCallbackSet(memContext, testFree, memContext), "    set callback");
        TEST_ERROR(memContextFree(memContext), AssertError, "error in callback");
    }

    // *****************************************************************************************************************************
    if (testBegin("MEM_CONTEXT_BEGIN() and MEM_CONTEXT_END()"))
    {
        memContextSwitch(memContextTop());
        MemContext *memContext = memContextNewP(
            "test-block", .childQty = MEM_CONTEXT_QTY_MAX, .allocQty = MEM_CONTEXT_QTY_MAX, .callbackQty = 1);
        memContextKeep();

        // Check normal block
        MEM_CONTEXT_BEGIN(memContext)
        {
            TEST_RESULT_Z(memContextCurrent()->name, "test-block", "context is now test-block");
        }
        MEM_CONTEXT_END();

        TEST_RESULT_Z(memContextCurrent()->name, "TOP", "context is now top");

        // Check block that errors
        TEST_ERROR(
            // {uncrustify_off - indentation}
            MEM_CONTEXT_BEGIN(memContext)
            {
                TEST_RESULT_Z(memContextCurrent()->name, "test-block", "context is now test-block");
                THROW(AssertError, "error in test block");
            }
            MEM_CONTEXT_END(),
            // {uncrustify_on}
            AssertError, "error in test block");

        TEST_RESULT_Z(memContextCurrent()->name, "TOP", "context is now top");

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

        MEM_CONTEXT_NEW_BEGIN(test-new-block, .childQty = MEM_CONTEXT_QTY_MAX, .allocQty = MEM_CONTEXT_QTY_MAX)
        {
            memContext = MEM_CONTEXT_NEW();
            TEST_RESULT_PTR(memContext, memContextCurrent(), "new mem context is current");
            TEST_RESULT_Z(memContext->name, memContextTestName, "check context name");
        }
        MEM_CONTEXT_NEW_END();

        TEST_RESULT_Z(memContextCurrent()->name, "TOP", "context name is now 'TOP'");
        TEST_RESULT_PTR(memContextCurrent(), memContextTop(), "context is now 'TOP'");
        TEST_RESULT_BOOL(memContext->active, true, "new mem context is still active");
        memContextFree(memContext);

        // ------------------------------------------------------------------------------------------------------------------------
        // Failed context new block
        memContextTestName = "test-new-failed-block";
        bool catch = false;

        TRY_BEGIN()
        {
            MEM_CONTEXT_NEW_BEGIN(test-new-failed-block, .childQty = MEM_CONTEXT_QTY_MAX, .allocQty = MEM_CONTEXT_QTY_MAX)
            {
                memContext = MEM_CONTEXT_NEW();
                TEST_RESULT_Z(memContext->name, memContextTestName, "check mem context name");
                THROW(FormatError, "create failed");
            }
            MEM_CONTEXT_NEW_END();
        }
        CATCH(FormatError)
        {
            catch = true;
        }
        TRY_END();

        TEST_RESULT_BOOL(catch, true, "new context error was caught");
        TEST_RESULT_PTR(memContextCurrent(), memContextTop(), "context is now 'TOP'");

        // ------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("new context not freed on fatal error");

        MemContext *volatile memContextFatal;
        catch = false;

        TRY_BEGIN()
        {
            MEM_CONTEXT_NEW_BEGIN(test-new-failed-fatal-block, .childQty = MEM_CONTEXT_QTY_MAX, .allocQty = MEM_CONTEXT_QTY_MAX)
            {
                memContextFatal = MEM_CONTEXT_NEW();
                THROW(AssertError, "create failed");
            }
            MEM_CONTEXT_NEW_END();
        }
        CATCH_FATAL()
        {
            catch = true;
        }
        TRY_END();

        TEST_RESULT_VOID(memContextFree(memContextFatal), "free new context not freed by catch fatal");
        TEST_RESULT_BOOL(catch, true, "new context error was caught");
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

        MEM_CONTEXT_NEW_BEGIN("outer", .childQty = MEM_CONTEXT_QTY_MAX)
        {
            MEM_CONTEXT_TEMP_BEGIN()
            {
                memContextNewP("not-to-be-moved", .childQty = MEM_CONTEXT_QTY_MAX);
                memContextKeep();

                MEM_CONTEXT_NEW_BEGIN("inner", .allocQty = MEM_CONTEXT_QTY_MAX)
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
                MEM_CONTEXT_NEW_BEGIN("inner2", .allocQty = MEM_CONTEXT_QTY_MAX)
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

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("outer and inner contexts allow one child");

        MemContext *memContextParent1;
        TEST_ASSIGN(memContextParent1, memContextNewP("parent1", .childQty = 1), "new parent1");
        TEST_RESULT_VOID(memContextKeep(), "keep parent1");

        TEST_RESULT_VOID(memContextSwitch(memContextParent1), "switch to parent1");

        MemContext *memContextChild;
        TEST_ASSIGN(memContextChild, memContextNewP("child", .allocQty = 0), "new child");
        TEST_RESULT_VOID(memContextKeep(), "keep child");

        TEST_RESULT_VOID(memContextSwitch(memContextTop()), "switch to top");

        MemContext *memContextParent2;
        TEST_ASSIGN(memContextParent2, memContextNewP("parent2", .childQty = 1), "new parent2");
        TEST_RESULT_VOID(memContextKeep(), "keep parent2");

        TEST_RESULT_VOID(memContextMove(memContextChild, memContextParent2), "move");
        TEST_RESULT_PTR(memContextChildOne(memContextParent1)->context, NULL, "check parent1");
        TEST_RESULT_PTR(memContextChildOne(memContextParent2)->context, memContextChild, "check parent2");
    }

    // *****************************************************************************************************************************
    if (testBegin("memContextAudit*s()"))
    {
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("one child context void success with no child context");

        MemContext *parent = memContextNewP("to be renamed", .allocExtra = 8, .childQty = 1);
        memContextKeep();
        memContextSwitch(parent);

        TEST_RESULT_VOID(memContextAuditAllocExtraName(memContextAllocExtra(parent), "one context child"), "rename context");
        TEST_RESULT_Z(parent->name, "one context child", "check name");

        MemContextAuditState audit = {.memContext = memContextCurrent()};

        TEST_RESULT_VOID(memContextAuditBegin(&audit), "audit begin");
        TEST_RESULT_VOID(memContextAuditEnd(&audit, "void"), "audit end");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("return immediately on any");

        audit = (MemContextAuditState){.memContext = memContextCurrent()};

        TEST_RESULT_VOID(memContextAuditBegin(&audit), "audit begin");
        audit.returnTypeAny = true;
        TEST_RESULT_VOID(memContextAuditEnd(&audit, "void"), "audit end");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("one child context void success");

        MemContext *child = memContextNewP("child", .childQty = 1);
        memContextKeep();

        audit = (MemContextAuditState){.memContext = memContextCurrent()};

        TEST_RESULT_VOID(memContextAuditBegin(&audit), "audit begin");
        TEST_RESULT_VOID(memContextAuditEnd(&audit, "void"), "audit end");

        memContextFree(child);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("one child context void success (freed context)");

        audit = (MemContextAuditState){.memContext = memContextCurrent()};

        TEST_RESULT_VOID(memContextAuditBegin(&audit), "audit begin");
        TEST_RESULT_VOID(memContextAuditEnd(&audit, "void"), "audit end");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("one child context match success");

        audit = (MemContextAuditState){.memContext = memContextCurrent()};
        TEST_RESULT_VOID(memContextAuditBegin(&audit), "audit begin");

        child = memContextNewP("child", .childQty = 1);
        memContextKeep();

        TEST_RESULT_VOID(memContextAuditEnd(&audit, "child"), "audit end");
        TEST_RESULT_VOID(memContextAuditEnd(&audit, "child *"), "audit end");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("one child context match failure");

        TEST_ERROR(memContextAuditEnd(&audit, "child2"), AssertError, "expected return type 'child2' but found 'child'");
        TEST_ERROR(memContextAuditEnd(&audit, "chil"), AssertError, "expected return type 'chil' but found 'child'");

        memContextSwitchBack();

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("many child context void success with no child context");

        parent = memContextNewP("many context child", .childQty = MEM_CONTEXT_QTY_MAX);
        memContextKeep();
        memContextSwitch(parent);

        audit = (MemContextAuditState){.memContext = memContextCurrent()};

        TEST_RESULT_VOID(memContextAuditBegin(&audit), "audit begin");
        TEST_RESULT_VOID(memContextAuditEnd(&audit, "void"), "audit end");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("many child context void success");

        child = memContextNewP("child", .childQty = 1);
        memContextKeep();

        audit = (MemContextAuditState){.memContext = memContextCurrent()};

        TEST_RESULT_VOID(memContextAuditBegin(&audit), "audit begin");
        TEST_RESULT_VOID(memContextAuditEnd(&audit, "void"), "audit end");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("many child context success with child context created before");

        audit = (MemContextAuditState){.memContext = memContextCurrent()};

        TEST_RESULT_VOID(memContextAuditBegin(&audit), "audit begin");
        TEST_RESULT_VOID(memContextAuditEnd(&audit, "child"), "audit end");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("many child context failure with multiple matches");

        audit = (MemContextAuditState){.memContext = memContextCurrent()};

        TEST_RESULT_VOID(memContextAuditBegin(&audit), "audit begin");

        memContextNewP("child2::extra", .childQty = 1);
        memContextKeep();

        memContextFree(child);
        memContextNewP("child2::extra", .childQty = 1);
        memContextKeep();

        TEST_ERROR(
            memContextAuditEnd(&audit, "child2"), AssertError,
            "expected return type 'child2::extra' already found but also found 'child2::extra'");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("many child context failure with no match");

        audit = (MemContextAuditState){.memContext = memContextCurrent()};

        TEST_RESULT_VOID(memContextAuditBegin(&audit), "audit begin");

        memContextNewP("child3::extra", .childQty = 1);
        memContextKeep();

        TEST_ERROR(
            memContextAuditEnd(&audit, "child2"), AssertError,
            "expected return type 'child2' but found 'child3::extra'");

        memContextSwitchBack();
    }

    memContextFree(memContextTop());

    FUNCTION_HARNESS_RETURN_VOID();
}
