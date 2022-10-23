/***********************************************************************************************************************************
Block Part Filter
***********************************************************************************************************************************/
#include "build.auto.h"

#include "command/backup/blockPartWrite.h"
// #include "common/compress/helper.h"
// #include "common/crypto/hash.h"
#include "common/debug.h"
// #include "common/io/bufferRead.h"
// #include "common/io/bufferWrite.h"
// #include "common/io/filter/size.h"
#include "common/log.h"
// #include "common/type/pack.h"
#include "common/type/object.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct BlockPartWrite
{
    MemContext *memContext;                                         // Mem context of filter

    const uint8_t *buffer;                                          // Internal buffer
    size_t bufferSize;                                              // Buffer size
    size_t bufferOffset;                                            // Buffer offset
    size_t sizeLast;                                                // Size of last part
    bool done;                                                      // Is the filter done?
    uint8_t header[CVT_VARINT128_BUFFER_SIZE];                      // Part header (block no, part size)
} BlockPartWrite;

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_BLOCK_PART_WRITE_TYPE                                                                                               \
    BlockPartWrite *
#define FUNCTION_LOG_BLOCK_PART_WRITE_FORMAT(value, buffer, bufferSize)                                                                  \
    objToLog(value, "BlockPartWrite", buffer, bufferSize)

/***********************************************************************************************************************************
Should the same input be provided again?
***********************************************************************************************************************************/
static bool
blockPartWriteInputSame(const THIS_VOID)
{
    THIS(const BlockPartWrite);

    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(BLOCK_PART_WRITE, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(BOOL, this->buffer != NULL);
}

/***********************************************************************************************************************************
Is filter done?
***********************************************************************************************************************************/
static bool
blockPartWriteDone(const THIS_VOID)
{
    THIS(const BlockPartWrite);

    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(BLOCK_PART_WRITE, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(BOOL, this->done && !blockPartWriteInputSame(this));
}

/***********************************************************************************************************************************
Count bytes in the input
***********************************************************************************************************************************/
static void
blockPartWriteProcess(THIS_VOID, const Buffer *const input, Buffer *const output)
{
    THIS(BlockPartWrite);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(BLOCK_PART_WRITE, this);
        FUNCTION_LOG_PARAM(BUFFER, input);
        FUNCTION_LOG_PARAM(BUFFER, output);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(output != NULL);

    // Input to process
    if (input != NULL)
    {
        // Write the part size
        if (this->buffer == NULL)
        {
            this->buffer = this->header;
            this->bufferSize = 0;
            this->bufferOffset = 0;

            // Write part size
            cvtUInt64ToVarInt128(
                this->sizeLast == 0 ? bufUsed(input) : cvtInt64ToZigZag((int64_t)bufUsed(input) - (int64_t)this->sizeLast) + 1,
                this->header, &this->bufferSize, SIZE_OF_STRUCT_MEMBER(BlockPartWrite, header));

            this->sizeLast = bufUsed(input);
        }

        do
        {
            if (bufRemains(output) >= this->bufferSize - this->bufferOffset)
            {
                bufCatC(output, this->buffer, this->bufferOffset, this->bufferSize - this->bufferOffset);

                if (this->buffer == this->header)
                {
                    this->buffer = bufPtrConst(input);
                    this->bufferSize = bufUsed(input);
                    this->bufferOffset = 0;
                }
                else
                    this->buffer = NULL;
            }
            else
            {
                const size_t outputSize = bufRemains(output);
                bufCatC(output, this->buffer, this->bufferOffset, outputSize);

                this->bufferOffset += outputSize;
            }
        }
        while (blockPartWriteInputSame(this) && !bufFull(output));
    }
    // Processing complete
    else
    {
        ASSERT(bufRemains(output) > 0);

        *(bufPtr(output) + bufUsed(output)) = '\0';
        bufUsedInc(output, 1);

        this->done = true;
    }

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
IoFilter *
blockPartWriteNew(void)
{
    FUNCTION_LOG_VOID(logLevelTrace);

    IoFilter *this = NULL;

    OBJ_NEW_BEGIN(BlockPartWrite, .childQty = MEM_CONTEXT_QTY_MAX, .allocQty = MEM_CONTEXT_QTY_MAX)
    {
        BlockPartWrite *const driver = OBJ_NEW_ALLOC();

        *driver = (BlockPartWrite)
        {
            .memContext = memContextCurrent(),
        };

        this = ioFilterNewP(
            BLOCK_PART_WRITE_FILTER_TYPE, driver, NULL, .done = blockPartWriteDone, .inOut = blockPartWriteProcess,
            .inputSame = blockPartWriteInputSame);
    }
    OBJ_NEW_END();

    FUNCTION_LOG_RETURN(IO_FILTER, this);
}
