/***********************************************************************************************************************************
Blob Handler
***********************************************************************************************************************************/
#include "build.auto.h"

#include <string.h>

#include "common/debug.h"
#include "common/memContext.h"
#include "common/type/blob.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct Blob
{
    char *block;                                                    // Current block for writing
    size_t pos;                                                     // Position in current block
};

/**********************************************************************************************************************************/
Blob *
blbNew(void)
{
    FUNCTION_TEST_VOID();

    Blob *this = NULL;

    OBJ_NEW_BEGIN(Blob, .allocQty = MEM_CONTEXT_QTY_MAX)
    {
        this = OBJ_NEW_ALLOC();

        *this = (Blob)
        {
            .block = memNew(BLOB_BLOCK_SIZE),
        };
    }
    OBJ_NEW_END();

    FUNCTION_TEST_RETURN(BLOB, this);
}

/**********************************************************************************************************************************/
const void *
blbAdd(Blob *const this, const void *const data, const size_t size)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(BLOB, this);
        FUNCTION_TEST_PARAM_P(VOID, data);
        FUNCTION_TEST_PARAM(SIZE, size);
    FUNCTION_TEST_END();

    void *result = NULL;

    MEM_CONTEXT_OBJ_BEGIN(this)
    {
        // If data is >= block size then allocate a special block for the data
        if (size >= BLOB_BLOCK_SIZE)
        {
            result = memNew(size);
            memcpy(result, data, size);
        }
        else
        {
            // If data will fit in the current block
            if (BLOB_BLOCK_SIZE - this->pos >= size)
            {
                result = (unsigned char *)this->block + this->pos;

                memcpy(result, data, size);
                this->pos += size;
            }
            // Else allocate a new block
            else
            {
                this->block = memNew(BLOB_BLOCK_SIZE);
                result = this->block;

                memcpy(result, data, size);
                this->pos = size;
            }
        }
    }
    MEM_CONTEXT_OBJ_END();

    FUNCTION_TEST_RETURN_P(VOID, result);

}
