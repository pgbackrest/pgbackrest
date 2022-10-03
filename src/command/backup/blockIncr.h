/***********************************************************************************************************************************
Block Incremental Filter

The block incremental filter builds a block list and map (see BlockMap object) either from a file (the first time) or from a file
and a prior map (each subsequent time). The block list is stored first and the block map second. The filter returns the size of the
block map so it can be extracted separately. The block list is useless without the map.

The map is duplicated in each backup where the file has changed so the block map only needs to be retrieved from the most recent
backup. However, the block map may reference block lists (or parts thereof) in any prior (or the current) backup.
***********************************************************************************************************************************/
#ifndef COMMAND_BACKUP_BLOCK_INCR_H
#define COMMAND_BACKUP_BLOCK_INCR_H

#include "common/io/filter/filter.h"

/***********************************************************************************************************************************
Filter type constant
***********************************************************************************************************************************/
#define BLOCK_INCR_FILTER_TYPE                                      STRID5("blk-incr", 0x90dc9dad820)

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
IoFilter *blockIncrNew(
    size_t blockSize, unsigned int reference, uint64_t bundleId, uint64_t bundleOffset, const Buffer *blockMapPrior);
IoFilter *blockIncrNewPack(const Pack *paramList);

#endif
