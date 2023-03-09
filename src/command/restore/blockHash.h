/***********************************************************************************************************************************
Block Hash List

Build a list of hashes based on a block size. This is used to compare the contents of a file to block map to determine what needs to
be updated.
***********************************************************************************************************************************/
#ifndef COMMAND_RESTORE_BLOCK_HASH_H
#define COMMAND_RESTORE_BLOCK_HASH_H

#include "common/io/filter/filter.h"
#include "common/type/stringId.h"

/***********************************************************************************************************************************
Filter type constant
***********************************************************************************************************************************/
#define BLOCK_HASH_FILTER_TYPE                                      STRID5("dlt-map", 0x402ddd1840)

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
FN_EXTERN IoFilter *blockHashNew(size_t blockSize, size_t checksumSize);

#endif
