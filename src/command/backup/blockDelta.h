/***********************************************************************************************************************************
Block Delta

!!!
***********************************************************************************************************************************/
#ifndef COMMAND_BACKUP_BLOCKDELTA_H
#define COMMAND_BACKUP_BLOCKDELTA_H

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct BlockDelta BlockDelta;

typedef struct BlockDeltaRead
{
    unsigned int reference;                                         // Reference to read from
    uint64_t bundleId;                                              // Bundle to read from
    uint64_t offset;                                                // Offset to begin read from
    uint64_t size;                                                  // Size of the read
    List *superBlockList;                                           // Super block list
} BlockDeltaRead;

typedef struct BlockDeltaSuperBlock
{
    uint64_t size;                                                  // Size of super block
    List *blockList;                                                // Block list
} BlockDeltaSuperBlock;

typedef struct BlockDeltaBlock
{
    uint64_t offset;                                                // Offset of block into super block
    unsigned char checksum[HASH_TYPE_SHA1_SIZE];                    // Checksum of the block
} BlockDeltaBlock;

#include "command/backup/blockMap.h"

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
FN_INLINE_ALWAYS BlockDelta *blockDeltaNew(const BlockMap *blockMap, size_t blockSize);

/***********************************************************************************************************************************
Getters/Setters
***********************************************************************************************************************************/
// Get a read item
FN_INLINE_ALWAYS const BlockDeltaRead *
blockDeltaReadGet(const BlockDelta *const this, const unsigned int readIdx)
{
    return (BlockDeltaRead *)lstGet((List *const)this, readIdx);
}

// Read list size
FN_INLINE_ALWAYS unsigned int
blockDeltaReadSize(const BlockDelta *const this)
{
    return lstSize((List *const)this);
}

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
FN_INLINE_ALWAYS void
blockDeltaFree(BlockMap *const this)
{
    objFree(this);
}

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_BLOCK_DELTA_TYPE                                                                                              \
    BlockDelta *
#define FUNCTION_LOG_BLOCK_DELTA_FORMAT(value, buffer, bufferSize)                                                                 \
    objNameToLog(value, "BlockDelta", buffer, bufferSize)

#endif
