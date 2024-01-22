/***********************************************************************************************************************************
Block Restore

Calculate and return the blocks required to restore a file using an optional block checksum list. The block checksum list is
optional because the file to restore may not exist so all the blocks will need to be restored.
***********************************************************************************************************************************/
#ifndef COMMAND_BACKUP_BLOCKDELTA_H
#define COMMAND_BACKUP_BLOCKDELTA_H

#include "command/backup/blockMap.h"
#include "common/compress/helper.h"
#include "common/crypto/common.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct BlockDelta BlockDelta;

// Reads that must be performed in order to extract blocks
typedef struct BlockDeltaRead
{
    unsigned int reference;                                         // Reference to read from
    uint64_t bundleId;                                              // Bundle to read from
    uint64_t offset;                                                // Offset to begin read from
    uint64_t size;                                                  // Size of the read
    List *superBlockList;                                           // Super block list
} BlockDeltaRead;

// Writes that need to be performed to restore the file
typedef struct BlockDeltaWrite
{
    uint64_t offset;                                                // Offset for the write
    Buffer *block;                                                  // Block to write
} BlockDeltaWrite;

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
FN_EXTERN BlockDelta *blockDeltaNew(
    const BlockMap *blockMap, size_t blockSize, size_t checksumSize, const Buffer *blockChecksum, CipherType cipherType,
    const String *cipherPass, const CompressType compressType);

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Get the next write for the restore
FN_EXTERN const BlockDeltaWrite *blockDeltaNext(BlockDelta *this, const BlockDeltaRead *readDelta, IoRead *readIo);

/***********************************************************************************************************************************
Getters/Setters
***********************************************************************************************************************************/
typedef struct BlockDeltaPub
{
    List *readList;                                                 // Read list
} BlockDeltaPub;

// Get read info
FN_INLINE_ALWAYS const BlockDeltaRead *
blockDeltaReadGet(const BlockDelta *const this, const unsigned int readIdx)
{
    return (BlockDeltaRead *)lstGet(THIS_PUB(BlockDelta)->readList, readIdx);
}

// Read list size
FN_INLINE_ALWAYS unsigned int
blockDeltaReadSize(const BlockDelta *const this)
{
    return lstSize(THIS_PUB(BlockDelta)->readList);
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
