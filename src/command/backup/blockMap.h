/***********************************************************************************************************************************
Block Incremental Map
***********************************************************************************************************************************/
#ifndef COMMAND_BACKUP_BLOCKMAP_H
#define COMMAND_BACKUP_BLOCKMAP_H

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct BlockMap BlockMap;

#include "common/crypto/hash.h"
#include "common/type/list.h"
#include "common/type/object.h"

typedef struct BlockMapItem
{
    unsigned int reference;
    unsigned char checksum[HASH_TYPE_SHA1_SIZE];
    uint64_t bundleId;
    uint64_t offset;
} BlockMapItem;

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
// Create empty block map
__attribute__((always_inline)) static inline BlockMap *
blockMapNew(void)
{
    return (BlockMap *)lstNewP(sizeof(BlockMapItem));
}

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Add a block map item
__attribute__((always_inline)) static inline const BlockMapItem *
blockMapAdd(BlockMap *const this, const BlockMapItem *const item)
{
    return (BlockMapItem *)lstAdd((List *const)this, item);
}

/***********************************************************************************************************************************
Getters/Setters
***********************************************************************************************************************************/
// Get a block map item
__attribute__((always_inline)) static inline const BlockMapItem *
blockMapGet(const BlockMap *const this, const unsigned int mapIdx)
{
    return (BlockMapItem *)lstGet((List *const)this, mapIdx);
}

// Block map size
__attribute__((always_inline)) static inline unsigned int
blockMapSize(const BlockMap *const this)
{
    return lstSize((List *const)this);
}

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
__attribute__((always_inline)) static inline void
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
    objToLog(value, "BlockMap", buffer, bufferSize)

#endif
