/***********************************************************************************************************************************
Block Incremental Map

The block incremental map stores the location of blocks of data that have been backed up incrementally. When a file changes, instead
of copying the entire file, just the blocks that have been changed can be stored. This map does not store the blocks themselves,
just the location where they can be found. It must be combined with a super block list to be useful (see BlockIncr filter).
***********************************************************************************************************************************/
#ifndef COMMAND_BACKUP_BLOCKMAP_H
#define COMMAND_BACKUP_BLOCKMAP_H

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct BlockMap BlockMap;

#include "common/crypto/xxhash.h"
#include "common/type/list.h"
#include "common/type/object.h"

typedef struct BlockMapItem
{
    unsigned int reference;                                         // Reference to backup where the block is stored
    uint64_t superBlockSize;                                        // Super block size
    uint64_t bundleId;                                              // Bundle where the block is stored (0 if not bundled)
    uint64_t offset;                                                // Offset of super block into the bundle
    uint64_t size;                                                  // Stored super block size (with compression, etc.)
    uint64_t block;                                                 // Block no inside of super block
    unsigned char checksum[XX_HASH_SIZE_MAX];                       // Checksum of the block
} BlockMapItem;

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
// Create empty block map
FN_INLINE_ALWAYS BlockMap *
blockMapNew(void)
{
    return (BlockMap *)OBJ_NAME(lstNewP(sizeof(BlockMapItem)), BlockMap::List);
}

// New block map from IO
FN_EXTERN BlockMap *blockMapNewRead(IoRead *map, size_t blockSize, size_t checksumSize);

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Add a block map item
FN_INLINE_ALWAYS BlockMapItem *
blockMapAdd(BlockMap *const this, const BlockMapItem *const item)
{
    ASSERT_INLINE(item != NULL);
    return (BlockMapItem *)lstAdd((List *const)this, item);
}

// Write map to IO
FN_EXTERN void blockMapWrite(const BlockMap *this, IoWrite *output, size_t blockSize, size_t checksumSize);

/***********************************************************************************************************************************
Getters/Setters
***********************************************************************************************************************************/
// Get a block map item
FN_INLINE_ALWAYS BlockMapItem *
blockMapGet(const BlockMap *const this, const unsigned int mapIdx)
{
    return (BlockMapItem *)lstGet((const List *const)this, mapIdx);
}

// Block map size
FN_INLINE_ALWAYS unsigned int
blockMapSize(const BlockMap *const this)
{
    return lstSize((const List *const)this);
}

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
FN_INLINE_ALWAYS void
blockMapFree(BlockMap *const this)
{
    objFree(this);
}

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_BLOCK_MAP_TYPE                                                                                                \
    BlockMap *
#define FUNCTION_LOG_BLOCK_MAP_FORMAT(value, buffer, bufferSize)                                                                   \
    objNameToLog(value, "BlockMap", buffer, bufferSize)

#endif
