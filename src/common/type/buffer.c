/***********************************************************************************************************************************
Buffer Handler
***********************************************************************************************************************************/
#include <stdio.h>
#include <string.h>

#include "common/assert.h"
#include "common/debug.h"
#include "common/type/buffer.h"

/***********************************************************************************************************************************
Contains information about the buffer
***********************************************************************************************************************************/
struct Buffer
{
    MemContext *memContext;
    size_t size;                                                    // Actual size of buffer
    bool limitSet;                                                  // Has a limit been set?
    size_t limit;                                                   // Limited reported size of the buffer to make it appear smaller
    size_t used;                                                    // Amount of buffer used
    unsigned char *buffer;                                          // Buffer allocation
};

/***********************************************************************************************************************************
Create a new buffer
***********************************************************************************************************************************/
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
        this->memContext = MEM_CONTEXT_NEW();
        this->size = size;
        this->used = 0;

        // Allocate buffer
        if (size > 0)
            this->buffer = memNewRaw(this->size);
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_TEST_RESULT(BUFFER, this);
}

/***********************************************************************************************************************************
Create a new buffer from a C buffer
***********************************************************************************************************************************/
Buffer *
bufNewC(size_t size, const void *buffer)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(SIZE, size);
        FUNCTION_TEST_PARAM(VOIDP, buffer);

        FUNCTION_TEST_ASSERT(buffer != NULL);
    FUNCTION_TEST_END();

    // Create object and copy data
    Buffer *this = bufNew(size);
    memcpy(this->buffer, buffer, this->size);
    this->used = this->size;

    FUNCTION_TEST_RESULT(BUFFER, this);
}

/***********************************************************************************************************************************
Create a new buffer from a string
***********************************************************************************************************************************/
Buffer *
bufNewStr(const String *string)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, string);

        FUNCTION_TEST_ASSERT(string != NULL);
    FUNCTION_TEST_END();

    // Create object and copy string
    Buffer *this = bufNew(strSize(string));
    memcpy(this->buffer, strPtr(string), bufSize(this));
    this->used = bufSize(this);

    FUNCTION_TEST_RESULT(BUFFER, this);
}

/***********************************************************************************************************************************
Create a new buffer from a zero-terminated string
***********************************************************************************************************************************/
Buffer *
bufNewZ(const char *string)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRINGZ, string);

        FUNCTION_TEST_ASSERT(string != NULL);
    FUNCTION_TEST_END();

    // Create a new buffer and then copy the string into it.
    Buffer *this = bufNew(strlen(string));
    memcpy(this->buffer, string, bufSize(this));
    this->used = bufSize(this);

    FUNCTION_TEST_RESULT(BUFFER, this);
}

/***********************************************************************************************************************************
Append the contents of another buffer
***********************************************************************************************************************************/
Buffer *
bufCat(Buffer *this, const Buffer *cat)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(BUFFER, this);
        FUNCTION_TEST_PARAM(BUFFER, cat);

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_TEST_END();

    if (cat != NULL)
        bufCatC(this, cat->buffer, 0, cat->used);

    FUNCTION_TEST_RESULT(BUFFER, this);
}

/***********************************************************************************************************************************
Append a C buffer
***********************************************************************************************************************************/
Buffer *
bufCatC(Buffer *this, const unsigned char *cat, size_t catOffset, size_t catSize)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(BUFFER, this);
        FUNCTION_TEST_PARAM(UCHARP, cat);
        FUNCTION_TEST_PARAM(SIZE, catOffset);
        FUNCTION_TEST_PARAM(SIZE, catSize);

        FUNCTION_TEST_ASSERT(this != NULL);
        FUNCTION_TEST_ASSERT(catSize == 0 || cat != NULL);
    FUNCTION_TEST_END();

    if (catSize > 0)
    {
        if (this->used + catSize > bufSize(this))
            bufResize(this, this->used + catSize);

        // Just here to silence nonnull warnings from clang static analyzer
        ASSERT_DEBUG(this->buffer != NULL);

        memcpy(this->buffer + this->used, cat + catOffset, catSize);
        this->used += catSize;
    }

    FUNCTION_TEST_RESULT(BUFFER, this);
}

/***********************************************************************************************************************************
Append a subset of another buffer
***********************************************************************************************************************************/
Buffer *
bufCatSub(Buffer *this, const Buffer *cat, size_t catOffset, size_t catSize)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(BUFFER, this);
        FUNCTION_TEST_PARAM(BUFFER, cat);
        FUNCTION_TEST_PARAM(SIZE, catOffset);
        FUNCTION_TEST_PARAM(SIZE, catSize);

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_TEST_END();

    if (cat != NULL)
    {
        ASSERT(catOffset <= cat->used);
        ASSERT(catSize <= cat->used - catOffset);

        bufCatC(this, cat->buffer, catOffset, catSize);
    }

    FUNCTION_TEST_RESULT(BUFFER, this);
}

/***********************************************************************************************************************************
Are two buffers equal?
***********************************************************************************************************************************/
bool
bufEq(const Buffer *this, const Buffer *compare)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(BUFFER, this);
        FUNCTION_TEST_PARAM(BUFFER, compare);

        FUNCTION_TEST_ASSERT(this != NULL);
        FUNCTION_TEST_ASSERT(compare != NULL);
    FUNCTION_TEST_END();

    bool result = false;

    if (this->used == compare->used)
        result = memcmp(this->buffer, compare->buffer, compare->used) == 0;

    FUNCTION_TEST_RESULT(BOOL, result);
}

/***********************************************************************************************************************************
Convert the buffer to a hex string
***********************************************************************************************************************************/
String *
bufHex(const Buffer *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(BUFFER, this);

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_TEST_END();

    String *result = strNew("");

    for (unsigned int bufferIdx = 0; bufferIdx < bufSize(this); bufferIdx++)
        strCatFmt(result, "%02x", this->buffer[bufferIdx]);

    FUNCTION_TEST_RESULT(STRING, result);
}

/***********************************************************************************************************************************
Move buffer to a new mem context
***********************************************************************************************************************************/
Buffer *
bufMove(Buffer *this, MemContext *parentNew)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(BUFFER, this);
        FUNCTION_TEST_PARAM(MEM_CONTEXT, parentNew);

        FUNCTION_TEST_ASSERT(parentNew != NULL);
    FUNCTION_TEST_END();

    if (this != NULL)
        memContextMove(this->memContext, parentNew);

    FUNCTION_TEST_RESULT(BUFFER, this);
}

/***********************************************************************************************************************************
Resize the buffer
***********************************************************************************************************************************/
Buffer *
bufResize(Buffer *this, size_t size)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(BUFFER, this);
        FUNCTION_TEST_PARAM(SIZE, size);

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_TEST_END();

    // Only resize if it the new size is different
    if (this->size != size)
    {
        // If new size is zero then free memory if allocated
        if (size == 0)
        {
            // When setting size down to 0 the buffer should always be allocated
            ASSERT_DEBUG(this->buffer != NULL);

            MEM_CONTEXT_BEGIN(this->memContext)
            {
                memFree(this->buffer);
            }
            MEM_CONTEXT_END();

            this->buffer = NULL;
            this->size = 0;
        }
        // Else allocate or resize
        else
        {
            MEM_CONTEXT_BEGIN(this->memContext)
            {
                if (this->buffer == NULL)
                    this->buffer = memNew(size);
                else
                    this->buffer = memGrowRaw(this->buffer, size);
            }
            MEM_CONTEXT_END();

            this->size = size;
        }

        if (this->used > this->size)
            this->used = this->size;

        if (this->limitSet && this->limit > this->size)
            this->limit = this->size;
    }

    FUNCTION_TEST_RESULT(BUFFER, this);
}

/***********************************************************************************************************************************
Is the buffer full?
***********************************************************************************************************************************/
bool
bufFull(const Buffer *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(BUFFER, this);

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RESULT(BOOL, this->used == bufSize(this));
}

/***********************************************************************************************************************************
Set and clear buffer limits
***********************************************************************************************************************************/
void
bufLimitClear(Buffer *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(BUFFER, this);

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_TEST_END();

    this->limitSet = false;

    FUNCTION_TEST_RESULT_VOID();
}

void
bufLimitSet(Buffer *this, size_t limit)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(BUFFER, this);
        FUNCTION_TEST_PARAM(SIZE, limit);

        FUNCTION_TEST_ASSERT(this != NULL);
        FUNCTION_TEST_ASSERT(limit <= this->size);
    FUNCTION_TEST_END();

    this->limit = limit;
    this->limitSet = true;

    FUNCTION_TEST_RESULT_VOID();
}

/***********************************************************************************************************************************
Return buffer ptr
***********************************************************************************************************************************/
unsigned char *
bufPtr(const Buffer *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(BUFFER, this);

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RESULT(UCHARP, this->buffer);
}

/***********************************************************************************************************************************
Get remaining space in the buffer
***********************************************************************************************************************************/
size_t
bufRemains(const Buffer *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(BUFFER, this);

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RESULT(SIZE, bufSize(this) - this->used);
}

/***********************************************************************************************************************************
Return pointer to remaining space in the buffer (after used space)
***********************************************************************************************************************************/
unsigned char *
bufRemainsPtr(const Buffer *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(BUFFER, this);

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RESULT(UCHARP, this->buffer + this->used);
}

/***********************************************************************************************************************************
Get buffer size
***********************************************************************************************************************************/
size_t
bufSize(const Buffer *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(BUFFER, this);

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RESULT(SIZE, this->limitSet ? this->limit : this->size);
}

/***********************************************************************************************************************************
Get/set the amount of the buffer actually used

Tracks how much of the buffer has actually been used.  This will be updated automatically when possible but if the buffer is
modified by using bufPtr() then the user is reponsible for updating the used size.
***********************************************************************************************************************************/
size_t
bufUsed(const Buffer *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(BUFFER, this);

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RESULT(SIZE, this->used);
}

void
bufUsedInc(Buffer *this, size_t inc)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(BUFFER, this);
        FUNCTION_TEST_PARAM(SIZE, inc);

        FUNCTION_TEST_ASSERT(this != NULL);
        FUNCTION_TEST_ASSERT(this->used + inc <= bufSize(this));
    FUNCTION_TEST_END();

    this->used += inc;

    FUNCTION_TEST_RESULT_VOID();
}

void
bufUsedSet(Buffer *this, size_t used)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(BUFFER, this);
        FUNCTION_TEST_PARAM(SIZE, used);

        FUNCTION_TEST_ASSERT(this != NULL);
        FUNCTION_TEST_ASSERT(used <= bufSize(this));
    FUNCTION_TEST_END();

    this->used = used;

    FUNCTION_TEST_RESULT_VOID();
}

void
bufUsedZero(Buffer *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(BUFFER, this);

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_TEST_END();

    this->used = 0;

    FUNCTION_TEST_RESULT_VOID();
}

/***********************************************************************************************************************************
Render as string for logging
***********************************************************************************************************************************/
String *
bufToLog(const Buffer *this)
{
    String *result = strNewFmt("{used: %zu, size: %zu, limit: ", this->used, this->size);

    if (this->limitSet)
        strCatFmt(result, "%zu}", this->limit);
    else
        strCat(result, "<off>}");

    return result;
}

/***********************************************************************************************************************************
Free the buffer
***********************************************************************************************************************************/
void
bufFree(Buffer *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(BUFFER, this);
    FUNCTION_TEST_END();

    if (this != NULL)
        memContextFree(this->memContext);

    FUNCTION_TEST_RESULT_VOID();
}
