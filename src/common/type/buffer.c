/***********************************************************************************************************************************
String Handler
***********************************************************************************************************************************/
#include <string.h>

#include "common/assert.h"
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

    return this;
}

/***********************************************************************************************************************************
Create a new buffer from a C buffer
***********************************************************************************************************************************/
Buffer *
bufNewC(size_t size, const void *buffer)
{
    // Create object
    Buffer *this = bufNew(size);

    // Copy the data
    memcpy(this->buffer, buffer, this->size);

    return this;
}

/***********************************************************************************************************************************
Create a new buffer from a string
***********************************************************************************************************************************/
Buffer *
bufNewStr(const String *string)
{
    // Create object
    Buffer *this = bufNew(strSize(string));

    // Copy the data
    memcpy(this->buffer, strPtr(string), this->size);

    return this;
}

/***********************************************************************************************************************************
Append the contents of another buffer
***********************************************************************************************************************************/
Buffer *
bufCat(Buffer *this, const Buffer *cat)
{
    ASSERT_DEBUG(this != NULL);

    if (cat != NULL && cat->size > 0)
    {
        size_t sizeOld = this->size;

        bufResize(this, sizeOld + cat->size);
        memcpy(this->buffer + sizeOld, cat->buffer, cat->size);
    }

    return this;
}

/***********************************************************************************************************************************
Are two buffers equal?
***********************************************************************************************************************************/
bool
bufEq(const Buffer *this, const Buffer *compare)
{
    bool result = false;

    ASSERT_DEBUG(this != NULL);
    ASSERT_DEBUG(compare != NULL);

    if (this->size == compare->size)
        result = memcmp(this->buffer, compare->buffer, compare->size) == 0;

    return result;
}

/***********************************************************************************************************************************
Move buffer to a new mem context
***********************************************************************************************************************************/
Buffer *
bufMove(Buffer *this, MemContext *parentNew)
{
    if (this != NULL)
        memContextMove(this->memContext, parentNew);

    return this;
}

/***********************************************************************************************************************************
Return buffer ptr
***********************************************************************************************************************************/
unsigned char *
bufPtr(const Buffer *this)
{
    return this->buffer;
}

/***********************************************************************************************************************************
Resize the buffer
***********************************************************************************************************************************/
Buffer *
bufResize(Buffer *this, size_t size)
{
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

    return this;
}

/***********************************************************************************************************************************
Return buffer size
***********************************************************************************************************************************/
size_t
bufSize(const Buffer *this)
{
    return this->size;
}

/***********************************************************************************************************************************
Free the buffer
***********************************************************************************************************************************/
void
bufFree(Buffer *this)
{
    if (this != NULL)
        memContextFree(this->memContext);
}
