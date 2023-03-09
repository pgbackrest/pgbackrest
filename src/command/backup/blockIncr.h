/***********************************************************************************************************************************
Block Incremental Filter

The block incremental filter builds a super block list and map (see BlockMap object) either from a file (the first time) or from a
file and a prior map (each subsequent time). The super block list is stored first and the block map second. The filter returns the
size of the block map so it can be extracted separately.

Super blocks contain a list of blocks that may or may not be contiguous. The advantage is that multiple blocks can be compressed
together which leads to better compression. The disadvantage is that the super block must be read sequentially to get at any block
in the super block. For large block sizes the block size will usually equal the super block size to negate the read penalty.

The map is duplicated in each backup where the file has changed so the block map only needs to be retrieved from the most recent
backup. However, the block map may reference super block lists (or parts thereof) in any prior (or the current) backup.

A super block consists of a series of blocks stored as chunks (see IoChunk filter) to make the format more flexible. Each block
consists of the following, compressed and encrypted as required:

- A varint-128 encoded block number stored as a delta of the prior block and left shifted by BLOCK_INCR_BLOCK_SHIFT. So, if the
  first block is 138 it would be stored as 138 and if the second block is 139 it would be stored as 1. Block numbers are only needed
  when the block list is being read sequentially, e.g. during verification. If blocks are accessed from the map then the block
  number is already known and the delta can be ignored.

- If the block number above has BLOCK_INCR_FLAG_SIZE set then the block size is different from the default and must be read. This is
  because it is at the end of the file and so also indicates the end of the super block.

- Block data.

The super block list is followed by the block map, which is encrypted separately when required but not compressed. The return value
of the filter is the stored block map size. Combined with the repo size this allows the block map to be read separately.

The block incremental should be read using BlockDelta since reconstructing the delta is quite involved.

The xxHash algorithm is used to determine which blocks have changed. A 128-bit xxHash is generated and then checksumSize bytes are
used from the hash depending on the size of the block. xxHash claims to have excellent dispersion characteristics, which has been
verified by testing with SMHasher and a custom test suite. xxHash-32 is used for up to 4MiB content blocks in lz4 and the lower
32-bits of xxHash-64 is used for content blocks in zstd. In general 32-bit checksums are pretty common defaults across filesystems
such as BtrFS and ZFS. We use at least 5 bytes even for the smallest blocks since we are looking for changes and not just
corruption. Ultimately if there is a collision and a block change is not detected it will almost certainly be caught by the overall
SHA1 file checksum. This will fail the backup, which is not ideal, but better than restoring corrupted data.
***********************************************************************************************************************************/
#ifndef COMMAND_BACKUP_BLOCK_INCR_H
#define COMMAND_BACKUP_BLOCK_INCR_H

#include "common/io/filter/filter.h"

/***********************************************************************************************************************************
Filter type constant
***********************************************************************************************************************************/
#define BLOCK_INCR_FILTER_TYPE                                      STRID5("blk-incr", 0x90dc9dad820)

/***********************************************************************************************************************************
Constants needed to read the block number in a super block
***********************************************************************************************************************************/
#define BLOCK_INCR_FLAG_SIZE                                        1 // Does the block have a different size than the default?
#define BLOCK_INCR_BLOCK_SHIFT                                      1 // Shift required to get block number

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
FN_EXTERN IoFilter *blockIncrNew(
    uint64_t superBlockSize, size_t blockSize, size_t checksumSize, unsigned int reference, uint64_t bundleId,
    uint64_t bundleOffset, const Buffer *blockMapPrior, const IoFilter *compress, const IoFilter *encrypt);
FN_EXTERN IoFilter *blockIncrNewPack(const Pack *paramList);

#endif
