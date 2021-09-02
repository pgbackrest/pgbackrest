/***********************************************************************************************************************************
Buffer Handler
***********************************************************************************************************************************/
#include "build.auto.h"

#include <stdio.h>
#include <string.h>

#include "common/debug.h"
#include "common/type/buffer.h"

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
    BufferPub pub;                                                  // Publicly accessible variables
};

/**********************************************************************************************************************************/
Buffer *
bufNew(size_t size)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(SIZE, size);
    FUNCTION_TEST_END();

    Buffer *this = NULL;

    OBJ_NEW_BEGIN(Buffer)
    {
        // Create object
        this = OBJ_NEW_ALLOC();

        *this = (Buffer)
        {
            .pub =
            {
                .sizeAlloc = size,
                .size = size,
            },
        };

        // Allocate buffer
        if (size > 0)
            this->pub.buffer = memNew(this->pub.sizeAlloc);
    }
    OBJ_NEW_END();

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
    memcpy(this->pub.buffer, buffer, bufSize(this));
    this->pub.used = bufSize(this);

    FUNCTION_TEST_RETURN(this);
}

/**********************************************************************************************************************************/
Buffer *
bufNewDecode(EncodeType type, const String *string)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, type);
        FUNCTION_TEST_PARAM(STRING, string);
    FUNCTION_TEST_END();

    Buffer *this = bufNew(decodeToBinSize(type, strZ(string)));

    decodeToBin(type, strZ(string), bufPtr(this));
    bufUsedSet(this, bufSize(this));

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
    Buffer *this = bufNew(buffer->pub.used);
    memcpy(this->pub.buffer, buffer->pub.buffer, bufSize(this));
    this->pub.used = bufSize(this);

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
        bufCatC(this, cat->pub.buffer, 0, cat->pub.used);

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
        if (bufUsed(this) + catSize > bufSize(this))
            bufResize(this, bufUsed(this) + catSize);

        // Just here to silence nonnull warnings from clang static analyzer
        ASSERT(bufPtr(this) != NULL);

        memcpy(bufPtr(this) + bufUsed(this), cat + catOffset, catSize);
        this->pub.used += catSize;
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
        ASSERT(catOffset <= cat->pub.used);
        ASSERT(catSize <= cat->pub.used - catOffset);

        bufCatC(this, cat->pub.buffer, catOffset, catSize);
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

    if (bufUsed(this) == bufUsed(compare))
        FUNCTION_TEST_RETURN(memcmp(bufPtrConst(this), bufPtrConst(compare), bufUsed(compare)) == 0);

    FUNCTION_TEST_RETURN(false);
}

/**********************************************************************************************************************************/
String *
bufHex(const Buffer *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(BUFFER, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    String *result = strNew();

    for (unsigned int bufferIdx = 0; bufferIdx < bufUsed(this); bufferIdx++)
        strCatFmt(result, "%02x", bufPtrConst(this)[bufferIdx]);

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
    if (bufSizeAlloc(this) != size)
    {
        // If new size is zero then free memory if allocated
        if (size == 0)
        {
            // When setting size down to 0 the buffer should always be allocated
            ASSERT(bufPtrConst(this) != NULL);

            MEM_CONTEXT_BEGIN(objMemContext(this))
            {
                memFree(bufPtr(this));
            }
            MEM_CONTEXT_END();

            this->pub.buffer = NULL;
            this->pub.sizeAlloc = 0;
        }
        // Else allocate or resize
        else
        {
            MEM_CONTEXT_BEGIN(objMemContext(this))
            {
                if (bufPtrConst(this) == NULL)
                    this->pub.buffer = memNew(size);
                else
                    this->pub.buffer = memResize(bufPtr(this), size);
            }
            MEM_CONTEXT_END();

            this->pub.sizeAlloc = size;
        }

        if (bufUsed(this) > bufSizeAlloc(this))
            this->pub.used = bufSizeAlloc(this);

        if (!bufSizeLimit(this))
            this->pub.size = bufSizeAlloc(this);
        else if (bufSize(this) > bufSizeAlloc(this))
            this->pub.size = bufSizeAlloc(this);
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

    this->pub.sizeLimit = false;
    this->pub.size = bufSizeAlloc(this);

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
    ASSERT(limit <= bufSizeAlloc(this));
    ASSERT(limit >= bufUsed(this));

    this->pub.size = limit;
    this->pub.sizeLimit = true;

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
    ASSERT(bufUsed(this) + inc <= bufSize(this));

    this->pub.used += inc;

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

    this->pub.used = used;

    FUNCTION_TEST_RETURN_VOID();
}

void
bufUsedZero(Buffer *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(BUFFER, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    this->pub.used = 0;

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
String *
bufToLog(const Buffer *this)
{
    String *result = strNewFmt(
        "{used: %zu, size: %zu%s", bufUsed(this), bufSize(this),
        bufSizeLimit(this) ? strZ(strNewFmt(", sizeAlloc: %zu}", bufSizeAlloc(this))) : "}");

    return result;
}
