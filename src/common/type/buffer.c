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
BUFFER_STRDEF_EXTERN(BRACEL_BUF,                                    "{");
BUFFER_STRDEF_EXTERN(BRACER_BUF,                                    "}");
BUFFER_STRDEF_EXTERN(BRACKETL_BUF,                                  "[");
BUFFER_STRDEF_EXTERN(BRACKETR_BUF,                                  "]");
BUFFER_STRDEF_EXTERN(COMMA_BUF,                                     ",");
BUFFER_STRDEF_EXTERN(EQ_BUF,                                        "=");
BUFFER_STRDEF_EXTERN(LF_BUF,                                        "\n");
BUFFER_STRDEF_EXTERN(QUOTED_BUF,                                    "\"");

/***********************************************************************************************************************************
Contains information about the buffer
***********************************************************************************************************************************/
struct Buffer
{
    BufferPub pub;                                                  // Publicly accessible variables
};

/**********************************************************************************************************************************/
FN_EXTERN Buffer *
bufNew(const size_t size)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(SIZE, size);
    FUNCTION_TEST_END();

    OBJ_NEW_BEGIN(Buffer, .allocQty = 1)
    {
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

    FUNCTION_TEST_RETURN(BUFFER, this);
}

/**********************************************************************************************************************************/
FN_EXTERN Buffer *
bufNewC(const void *const buffer, const size_t size)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM_P(VOID, buffer);
        FUNCTION_TEST_PARAM(SIZE, size);
    FUNCTION_TEST_END();

    ASSERT(buffer != NULL);

    // Create object and copy data
    Buffer *const this = bufNew(size);
    memcpy(this->pub.buffer, buffer, bufSize(this));
    this->pub.used = bufSize(this);

    FUNCTION_TEST_RETURN(BUFFER, this);
}

/**********************************************************************************************************************************/
FN_EXTERN Buffer *
bufNewDecode(const EncodingType type, const String *const string)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, type);
        FUNCTION_TEST_PARAM(STRING, string);
    FUNCTION_TEST_END();

    Buffer *const this = bufNew(decodeToBinSize(type, strZ(string)));

    decodeToBin(type, strZ(string), bufPtr(this));
    bufUsedSet(this, bufSize(this));

    FUNCTION_TEST_RETURN(BUFFER, this);
}

/**********************************************************************************************************************************/
FN_EXTERN Buffer *
bufDup(const Buffer *const buffer)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(BUFFER, buffer);
    FUNCTION_TEST_END();

    ASSERT(buffer != NULL);

    // Create object and copy data
    Buffer *const this = bufNew(bufUsed(buffer));

    if (bufUsed(buffer) != 0)
        memcpy(this->pub.buffer, buffer->pub.buffer, bufSize(this));

    this->pub.used = bufSize(this);

    FUNCTION_TEST_RETURN(BUFFER, this);
}

/**********************************************************************************************************************************/
FN_EXTERN Buffer *
bufCat(Buffer *const this, const Buffer *const cat)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(BUFFER, this);
        FUNCTION_TEST_PARAM(BUFFER, cat);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    if (cat != NULL)
        bufCatC(this, cat->pub.buffer, 0, bufUsed(cat));

    FUNCTION_TEST_RETURN(BUFFER, this);
}

/**********************************************************************************************************************************/
FN_EXTERN Buffer *
bufCatC(Buffer *const this, const unsigned char *const cat, const size_t catOffset, const size_t catSize)
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

    FUNCTION_TEST_RETURN(BUFFER, this);
}

/**********************************************************************************************************************************/
FN_EXTERN Buffer *
bufCatSub(Buffer *const this, const Buffer *const cat, const size_t catOffset, const size_t catSize)
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
        ASSERT(catOffset <= bufUsed(cat));
        ASSERT(catSize <= bufUsed(cat) - catOffset);

        bufCatC(this, cat->pub.buffer, catOffset, catSize);
    }

    FUNCTION_TEST_RETURN(BUFFER, this);
}

/**********************************************************************************************************************************/
FN_EXTERN bool
bufEq(const Buffer *const this, const Buffer *const compare)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(BUFFER, this);
        FUNCTION_TEST_PARAM(BUFFER, compare);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(compare != NULL);

    if (bufUsed(this) == bufUsed(compare))
        FUNCTION_TEST_RETURN(BOOL, memcmp(bufPtrConst(this), bufPtrConst(compare), bufUsed(compare)) == 0);

    FUNCTION_TEST_RETURN(BOOL, false);
}

/**********************************************************************************************************************************/
FN_EXTERN const unsigned char *
bufFind(const Buffer *const this, const Buffer *const find, const BufFindParam param)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(BUFFER, this);
        FUNCTION_TEST_PARAM(BUFFER, find);
        FUNCTION_TEST_PARAM_P(VOID, param.begin);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(find != NULL);
    ASSERT(param.begin == NULL || (param.begin >= bufPtrConst(this) && param.begin - bufPtrConst(this) <= (off_t)bufUsed(this)));

    const void *result = NULL;

    if (bufUsed(this) >= bufUsed(find))
    {
        const unsigned char *haystack = param.begin != NULL ? param.begin : bufPtrConst(this);
        unsigned int findIdx = (unsigned int)(haystack - bufPtrConst(this));

        for (; findIdx <= bufUsed(this) - bufUsed(find); findIdx++)
        {
            if (memcmp(haystack, bufPtrConst(find), bufSize(find)) == 0)
            {
                result = haystack;
                break;
            }

            haystack++;
        }
    }

    FUNCTION_TEST_RETURN_CONST_P(UCHARDATA, result);
}

/**********************************************************************************************************************************/
FN_EXTERN Buffer *
bufResize(Buffer *const this, const size_t size)
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

            MEM_CONTEXT_OBJ_BEGIN(this)
            {
                memFree(bufPtr(this));
            }
            MEM_CONTEXT_OBJ_END();

            this->pub.buffer = NULL;
            this->pub.sizeAlloc = 0;
        }
        // Else allocate or resize
        else
        {
            MEM_CONTEXT_OBJ_BEGIN(this)
            {
                if (bufPtrConst(this) == NULL)
                    this->pub.buffer = memNew(size);
                else
                    this->pub.buffer = memResize(bufPtr(this), size);
            }
            MEM_CONTEXT_OBJ_END();

            this->pub.sizeAlloc = size;
        }

        if (bufUsed(this) > bufSizeAlloc(this))
            this->pub.used = bufSizeAlloc(this);

        if (!bufSizeLimit(this))
            this->pub.size = bufSizeAlloc(this);
        else if (bufSize(this) > bufSizeAlloc(this))
            this->pub.size = bufSizeAlloc(this);
    }

    FUNCTION_TEST_RETURN(BUFFER, this);
}

/**********************************************************************************************************************************/
FN_EXTERN void
bufLimitClear(Buffer *const this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(BUFFER, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    this->pub.sizeLimit = false;
    this->pub.size = bufSizeAlloc(this);

    FUNCTION_TEST_RETURN_VOID();
}

FN_EXTERN void
bufLimitSet(Buffer *const this, const size_t limit)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(BUFFER, this);
        FUNCTION_TEST_PARAM(SIZE, limit);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(limit <= bufSizeAlloc(this));
    ASSERT(limit >= bufUsed(this));

    this->pub.size = limit;
    this->pub.sizeLimit = limit != bufSizeAlloc(this);

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
FN_EXTERN void
bufUsedInc(Buffer *const this, const size_t inc)
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

FN_EXTERN void
bufUsedSet(Buffer *const this, const size_t used)
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

FN_EXTERN void
bufUsedZero(Buffer *const this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(BUFFER, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    this->pub.used = 0;

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
FN_EXTERN void
bufToLog(const Buffer *const this, StringStatic *const debugLog)
{
    strStcFmt(debugLog, "{used: %zu, size: %zu", bufUsed(this), bufSize(this));

    if (bufSizeLimit(this))
        strStcFmt(debugLog, ", sizeAlloc: %zu", bufSizeAlloc(this));

    strStcCatChr(debugLog, '}');
}
