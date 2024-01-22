/***********************************************************************************************************************************
Harness for Testing Block Deltas
***********************************************************************************************************************************/
#ifndef TEST_COMMON_HARNESS_BLOCK_DELTA_H
#define TEST_COMMON_HARNESS_BLOCK_DELTA_H

#include "command/backup/blockMap.h"

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Render the block delta as text for testing
String *hrnBlockDeltaRender(const BlockMap *blockMap, size_t blockSize, size_t checksumSize);

#endif
