/***********************************************************************************************************************************
Block Incremental Filter

The block incremental filter builds a super block list and map (see BlockMap object) either from a file (the first time) or from a
file and a prior map (each subsequent time). The super block list is stored first and the block map second. The filter returns the
size of the block map so it can be extracted separately.

Super blocks contain a list of blocks that may or may not be contiguous. The advantage is that multiple blocks can be compressed
together which leads to better compression. The disadvantage is that the super block must be read sequentially to get at any block
in the super block. For large block sizes the block size will usually equal the super block size to negate the read penalty.

The super block list is followed by the block map, which is encrypted separately when required but not compressed. The return value
of the filter is the stored block map size. Combined with the repo size this allows the block map to be read separately.

The map is duplicated in each backup where the file has changed so the block map only needs to be retrieved from the most recent
backup. However, the block map may reference super block lists (or parts thereof) in any prior (or the current) backup.

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
Constructors
***********************************************************************************************************************************/
FN_EXTERN IoFilter *blockIncrNew(
    uint64_t superBlockSize, size_t blockSize, size_t checksumSize, unsigned int reference, uint64_t bundleId,
    uint64_t bundleOffset, const Buffer *blockMapPrior, const IoFilter *compress, const IoFilter *encrypt);
FN_EXTERN IoFilter *blockIncrNewPack(const Pack *paramList);

#endif
