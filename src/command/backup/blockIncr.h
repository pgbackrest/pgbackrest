/***********************************************************************************************************************************
Block Incremental Filter

The block incremental filter builds a block list and map (see BlockMap object) either from a file (the first time) or from a file
and a prior map (each subsequent time). The block list is stored first and the block map second. The filter returns the size of the
block map so it can be extracted separately.

The map is duplicated in each backup where the file has changed so the block map only needs to be retrieved from the most recent
backup. However, the block map may reference block lists (or parts thereof) in any prior (or the current) backup.

The block list consists of a series of blocks stored as chunks (see IoChunk filter) to make the format more flexible. Each block
consists of the following, compressed and encrypted as required:

- A varint-128 encoded block number stored as a delta of the previous block. So, if the first block is 138 it would be stored as 138
  and if the second block is 139 it would be stored as 1. Block numbers are only needed when the block list is being read
  sequentially, e.g. during verification. If blocks are accessed from the map then the block number is already known and the delta
  can be ignored.

- Block data.

The block list is followed by the block map, which is encrypted separately when required but not compressed. The return value of the
filter is the stored block size. Combined with the repo size this allows the block map to be read separately.
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
    size_t blockSize, unsigned int reference, uint64_t bundleId, uint64_t bundleOffset, const Buffer *blockMapPrior,
    const IoFilter *compress, const IoFilter *encrypt);
IoFilter *blockIncrNewPack(const Pack *paramList);

#endif
