/***********************************************************************************************************************************
Block Delta

!!!
***********************************************************************************************************************************/
#ifndef COMMAND_BACKUP_BLOCKDELTA_H
#define COMMAND_BACKUP_BLOCKDELTA_H

#include "command/backup/blockMap.h"
#include "common/compress/helper.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct BlockRestore BlockRestore;

typedef struct BlockRestoreRead
{
    unsigned int reference;                                         // Reference to read from
    uint64_t bundleId;                                              // Bundle to read from
    uint64_t offset;                                                // Offset to begin read from
    uint64_t size;                                                  // Size of the read
    List *superBlockList;                                           // Super block list
} BlockRestoreRead;

typedef struct BlockRestoreWrite
{
    uint64_t offset;                                                // Offset for the write
    Buffer *block;                                                  // Block to write
} BlockRestoreWrite;

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
FN_EXTERN BlockRestore *blockRestoreNew(
    const BlockMap *blockMap, size_t blockSize, const Buffer *deltaMap, CipherType cipherType, const String *cipherPass,
    const CompressType compressType);

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
FN_EXTERN const BlockRestoreWrite *blockRestoreNext(BlockRestore *this, const BlockRestoreRead *readDelta, IoRead *readIo);

/***********************************************************************************************************************************
Getters/Setters
***********************************************************************************************************************************/
typedef struct BlockRestorePub
{
    List *readList;                                                 // Read list
} BlockRestorePub;

// Get a read item
FN_INLINE_ALWAYS const BlockRestoreRead *
blockRestoreReadGet(const BlockRestore *const this, const unsigned int readIdx)
{
    return (BlockRestoreRead *)lstGet(THIS_PUB(BlockRestore)->readList, readIdx);
}

// Read list size
FN_INLINE_ALWAYS unsigned int
blockRestoreReadSize(const BlockRestore *const this)
{
    return lstSize(THIS_PUB(BlockRestore)->readList);
}

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
FN_INLINE_ALWAYS void
blockRestoreFree(BlockMap *const this)
{
    objFree(this);
}

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_BLOCK_DELTA_TYPE                                                                                              \
    BlockRestore *
#define FUNCTION_LOG_BLOCK_DELTA_FORMAT(value, buffer, bufferSize)                                                                 \
    objNameToLog(value, "BlockRestore", buffer, bufferSize)

#endif
