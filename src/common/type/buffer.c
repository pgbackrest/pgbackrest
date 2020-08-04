/***********************************************************************************************************************************
Buffer Handler
***********************************************************************************************************************************/
#include "build.auto.h"

#include <stdio.h>
#include <string.h>

#include "common/debug.h"
#include "common/type/buffer.h"
#include "common/type/object.h"

/***********************************************************************************************************************************
Constant buffers that are generally useful
***********************************************************************************************************************************/
BUFFER_STRDEF_EXTERN(BRACEL_BUF,                                    BRACEL_Z);
BUFFER_STRDEF_EXTERN(BRACER_BUF,                                    BRACER_Z);
BUFFER_STRDEF_EXTERN(BRACKETL_BUF,                                  BRACKETL_Z);
BUFFER_STRDEF_EXTERN(BRACKETR_BUF,                                  BRACKETR_Z);
BUFFER_STRDEF_EXTERN(COMMA_BUF,                                     COMMA_Z);
BUFFER_STRDEF_EXTERN(CR_BUF,                                        CR_Z);
BUFFER_STRDEF_EXTERN(DOT_BUF,                                       DOT_Z);
BUFFER_STRDEF_EXTERN(EQ_BUF,                                        EQ_Z);
BUFFER_STRDEF_EXTERN(LF_BUF,                                        LF_Z);
BUFFER_STRDEF_EXTERN(QUOTED_BUF,                                    QUOTED_Z);

/***********************************************************************************************************************************
Contains information about the buffer
***********************************************************************************************************************************/
struct Buffer
{
    BUFFER_COMMON                                                   // Variables that are common to static and dynamic buffers
    unsigned char *buffer;                                          // Internal buffer
    MemContext *memContext;                                         // Mem context for dynamic buffers
};

OBJECT_DEFINE_MOVE(BUFFER);
OBJECT_DEFINE_FREE(BUFFER);

/**********************************************************************************************************************************/
Buffer *
bufNew(size_t size)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(SIZE, size);
    FUNCTION_TEST_END();

    Buffer *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("Buffer")
    {
        // Create object
        this = memNew(sizeof(Buffer));

        *this = (Buffer)
        {
            .memContext = MEM_CONTEXT_NEW(),
            .sizeAlloc = size,
            .size = size,
        };

        // Allocate buffer
        if (size > 0)
            this->buffer = memNew(this->sizeAlloc);
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_TEST_RETURN(this);
}

/**********************************************************************************************************************************/
Buffer *
bufNewC(const void *buffer, size_t size)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM_P(VOID, buffer);
        FUNCTION_TEST_PARAM(SIZE, size);
    FUNCTION_TEST_END();

    ASSERT(buffer != NULL);

    // Create object and copy data
    Buffer *this = bufNew(size);
    memcpy(this->buffer, buffer, this->size);
    this->used = this->size;

    FUNCTION_TEST_RETURN(this);
}

/**********************************************************************************************************************************/
Buffer *
bufDup(const Buffer *buffer)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(BUFFER, buffer);
    FUNCTION_TEST_END();

    ASSERT(buffer != NULL);

    // Create object and copy data
    Buffer *this = bufNew(buffer->used);
    memcpy(this->buffer, buffer->buffer, this->size);
    this->used = this->size;

    FUNCTION_TEST_RETURN(this);
}

/**********************************************************************************************************************************/
Buffer *
bufCat(Buffer *this, const Buffer *cat)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(BUFFER, this);
        FUNCTION_TEST_PARAM(BUFFER, cat);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    if (cat != NULL)
        bufCatC(this, cat->buffer, 0, cat->used);

    FUNCTION_TEST_RETURN(this);
}

/**********************************************************************************************************************************/
Buffer *
bufCatC(Buffer *this, const unsigned char *cat, size_t catOffset, size_t catSize)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(BUFFER, this);
        FUNCTION_TEST_PARAM_P(UCHARDATA, cat);
        FUNCTION_TEST_PARAM(SIZE, catOffset);
        FUNCTION_TEST_PARAM(SIZE, catSize);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(catSize == 0 || cat != NULL);

    if (catSize > 0)
    {
        if (this->used + catSize > bufSize(this))
            bufResize(this, this->used + catSize);

        // Just here to silence nonnull warnings from clang static analyzer
        ASSERT(this->buffer != NULL);

        memcpy(this->buffer + this->used, cat + catOffset, catSize);
        this->used += catSize;
    }

    FUNCTION_TEST_RETURN(this);
}

/**********************************************************************************************************************************/
Buffer *
bufCatSub(Buffer *this, const Buffer *cat, size_t catOffset, size_t catSize)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(BUFFER, this);
        FUNCTION_TEST_PARAM(BUFFER, cat);
        FUNCTION_TEST_PARAM(SIZE, catOffset);
        FUNCTION_TEST_PARAM(SIZE, catSize);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    if (cat != NULL)
    {
        ASSERT(catOffset <= cat->used);
        ASSERT(catSize <= cat->used - catOffset);

        bufCatC(this, cat->buffer, catOffset, catSize);
    }

    FUNCTION_TEST_RETURN(this);
}

/**********************************************************************************************************************************/
bool
bufEq(const Buffer *this, const Buffer *compare)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(BUFFER, this);
        FUNCTION_TEST_PARAM(BUFFER, compare);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(compare != NULL);

    bool result = false;

    if (this->used == compare->used)
        result = memcmp(this->buffer, compare->buffer, compare->used) == 0;

    FUNCTION_TEST_RETURN(result);
}

/**********************************************************************************************************************************/
String *
bufHex(const Buffer *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(BUFFER, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    String *result = strNew("");

    for (unsigned int bufferIdx = 0; bufferIdx < bufUsed(this); bufferIdx++)
        strCatFmt(result, "%02x", this->buffer[bufferIdx]);

    FUNCTION_TEST_RETURN(result);
}

/**********************************************************************************************************************************/
Buffer *
bufResize(Buffer *this, size_t size)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(BUFFER, this);
        FUNCTION_TEST_PARAM(SIZE, size);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    // Only resize if it the new size is different
    if (this->sizeAlloc != size)
    {
        // If new size is zero then free memory if allocated
        if (size == 0)
        {
            // When setting size down to 0 the buffer should always be allocated
            ASSERT(this->buffer != NULL);

            MEM_CONTEXT_BEGIN(this->memContext)
            {
                memFree(this->buffer);
            }
            MEM_CONTEXT_END();

            this->buffer = NULL;
            this->sizeAlloc = 0;
        }
        // Else allocate or resize
        else
        {
            MEM_CONTEXT_BEGIN(this->memContext)
            {
                if (this->buffer == NULL)
                    this->buffer = memNew(size);
                else
                    this->buffer = memResize(this->buffer, size);
            }
            MEM_CONTEXT_END();

            this->sizeAlloc = size;
        }

        if (this->used > this->sizeAlloc)
            this->used = this->sizeAlloc;

        if (!this->sizeLimit)
            this->size = this->sizeAlloc;
        else if (this->size > this->sizeAlloc)
            this->size = this->sizeAlloc;
    }

    FUNCTION_TEST_RETURN(this);
}

/**********************************************************************************************************************************/
void
bufLimitClear(Buffer *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(BUFFER, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    this->sizeLimit = false;
    this->size = this->sizeAlloc;

    FUNCTION_TEST_RETURN_VOID();
}

void
bufLimitSet(Buffer *this, size_t limit)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(BUFFER, this);
        FUNCTION_TEST_PARAM(SIZE, limit);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(limit <= this->sizeAlloc);

    this->size = limit;
    this->sizeLimit = true;

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
void
bufUsedInc(Buffer *this, size_t inc)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(BUFFER, this);
        FUNCTION_TEST_PARAM(SIZE, inc);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(this->used + inc <= bufSize(this));

    this->used += inc;

    FUNCTION_TEST_RETURN_VOID();
}

void
bufUsedSet(Buffer *this, size_t used)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(BUFFER, this);
        FUNCTION_TEST_PARAM(SIZE, used);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(used <= bufSize(this));

    this->used = used;

    FUNCTION_TEST_RETURN_VOID();
}

void
bufUsedZero(Buffer *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(BUFFER, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    this->used = 0;

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
String *
bufToLog(const Buffer *this)
{
    String *result = strNewFmt(
        "{used: %zu, size: %zu%s", this->used, this->size,
        this->sizeLimit ? strZ(strNewFmt(", sizeAlloc: %zu}", this->sizeAlloc)) : "}");

    return result;
}
