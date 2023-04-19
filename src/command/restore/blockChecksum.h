/***********************************************************************************************************************************
Block Hash List

Build a list of hashes based on a block size. This is used to compare the contents of a file to block map to determine what needs to
be updated.
***********************************************************************************************************************************/
#ifndef COMMAND_RESTORE_BLOCK_CHECKSUM_H
#define COMMAND_RESTORE_BLOCK_CHECKSUM_H

#include "common/io/filter/filter.h"
#include "common/type/stringId.h"

/***********************************************************************************************************************************
Filter type constant
***********************************************************************************************************************************/
#define BLOCK_CHECKSUM_FILTER_TYPE                                  STRID5("blk-chk", 0x2d03dad820)

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
FN_EXTERN IoFilter *blockChecksumNew(size_t blockSize, size_t checksumSize);
FN_EXTERN IoFilter *blockChecksumNewPack(const Pack *paramList);

#endif
