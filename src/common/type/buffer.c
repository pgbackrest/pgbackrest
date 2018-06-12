/***********************************************************************************************************************************
String Handler
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
    size_t size;
    unsigned char *buffer;
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

    // Create object
    Buffer *this = bufNew(strSize(string));

    // Copy the data
    memcpy(this->buffer, strPtr(string), this->size);

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

    if (cat != NULL && cat->size > 0)
    {
        size_t sizeOld = this->size;

        bufResize(this, sizeOld + cat->size);

        // Just here to silence nonnull warnings from clang static analyzer
        ASSERT_DEBUG(this->buffer != NULL);

        memcpy(this->buffer + sizeOld, cat->buffer, cat->size);
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

    if (this->size == compare->size)
        result = memcmp(this->buffer, compare->buffer, compare->size) == 0;

    FUNCTION_TEST_RESULT(BOOL, result);
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
    }

    FUNCTION_TEST_RESULT(BUFFER, this);
}

/***********************************************************************************************************************************
Return buffer size
***********************************************************************************************************************************/
size_t
bufSize(const Buffer *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(BUFFER, this);

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RESULT(SIZE, this->size);
}

/***********************************************************************************************************************************
Convert to a zero-terminated string for logging
***********************************************************************************************************************************/
size_t
bufToLog(const Buffer *this, char *buffer, size_t bufferSize)
{
    size_t result = 0;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        String *string = NULL;

        if (this == NULL)
            string = strNew("null");
        else
            string = strNewFmt("{size: %zu}", this->size);

        result = (size_t)snprintf(buffer, bufferSize, "%s", strPtr(string));
    }
    MEM_CONTEXT_TEMP_END();

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
