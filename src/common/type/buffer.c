/***********************************************************************************************************************************
Buffer Handler
***********************************************************************************************************************************/
#include <stdio.h>
#include <string.h>

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

    FUNCTION_TEST_RETURN(this);
}

/***********************************************************************************************************************************
Create a new buffer from a C buffer
***********************************************************************************************************************************/
Buffer *
bufNewC(size_t size, const void *buffer)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(SIZE, size);
        FUNCTION_TEST_PARAM_P(VOID, buffer);
    FUNCTION_TEST_END();

    ASSERT(buffer != NULL);

    // Create object and copy data
    Buffer *this = bufNew(size);
    memcpy(this->buffer, buffer, this->size);
    this->used = this->size;

    FUNCTION_TEST_RETURN(this);
}

/***********************************************************************************************************************************
Create a new buffer from a string
***********************************************************************************************************************************/
Buffer *
bufNewStr(const String *string)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, string);
    FUNCTION_TEST_END();

    ASSERT(string != NULL);

    // Create object and copy string
    Buffer *this = bufNew(strSize(string));
    memcpy(this->buffer, strPtr(string), bufSize(this));
    this->used = bufSize(this);

    FUNCTION_TEST_RETURN(this);
}

/***********************************************************************************************************************************
Create a new buffer from a zero-terminated string
***********************************************************************************************************************************/
Buffer *
bufNewZ(const char *string)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRINGZ, string);
    FUNCTION_TEST_END();

    ASSERT(string != NULL);

    // Create a new buffer and then copy the string into it.
    Buffer *this = bufNew(strlen(string));
    memcpy(this->buffer, string, bufSize(this));
    this->used = bufSize(this);

    FUNCTION_TEST_RETURN(this);
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
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    if (cat != NULL)
        bufCatC(this, cat->buffer, 0, cat->used);

    FUNCTION_TEST_RETURN(this);
}

/***********************************************************************************************************************************
Append a C buffer
***********************************************************************************************************************************/
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

/***********************************************************************************************************************************
Are two buffers equal?
***********************************************************************************************************************************/
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

/***********************************************************************************************************************************
Convert the buffer to a hex string
***********************************************************************************************************************************/
String *
bufHex(const Buffer *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(BUFFER, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    String *result = strNew("");

    for (unsigned int bufferIdx = 0; bufferIdx < bufSize(this); bufferIdx++)
        strCatFmt(result, "%02x", this->buffer[bufferIdx]);

    FUNCTION_TEST_RETURN(result);
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
    FUNCTION_TEST_END();

    ASSERT(parentNew != NULL);

    if (this != NULL)
        memContextMove(this->memContext, parentNew);

    FUNCTION_TEST_RETURN(this);
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
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    // Only resize if it the new size is different
    if (this->size != size)
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

    FUNCTION_TEST_RETURN(this);
}

/***********************************************************************************************************************************
Is the buffer full?
***********************************************************************************************************************************/
bool
bufFull(const Buffer *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(BUFFER, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->used == bufSize(this));
}

/***********************************************************************************************************************************
Set and clear buffer limits
***********************************************************************************************************************************/
void
bufLimitClear(Buffer *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(BUFFER, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    this->limitSet = false;

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
    ASSERT(limit <= this->size);

    this->limit = limit;
    this->limitSet = true;

    FUNCTION_TEST_RETURN_VOID();
}

/***********************************************************************************************************************************
Return buffer ptr
***********************************************************************************************************************************/
unsigned char *
bufPtr(const Buffer *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(BUFFER, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->buffer);
}

/***********************************************************************************************************************************
Get remaining space in the buffer
***********************************************************************************************************************************/
size_t
bufRemains(const Buffer *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(BUFFER, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(bufSize(this) - this->used);
}

/***********************************************************************************************************************************
Return pointer to remaining space in the buffer (after used space)
***********************************************************************************************************************************/
unsigned char *
bufRemainsPtr(const Buffer *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(BUFFER, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->buffer + this->used);
}

/***********************************************************************************************************************************
Get buffer size
***********************************************************************************************************************************/
size_t
bufSize(const Buffer *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(BUFFER, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->limitSet ? this->limit : this->size);
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
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->used);
}

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

    FUNCTION_TEST_RETURN_VOID();
}
