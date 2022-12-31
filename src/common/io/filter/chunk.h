/***********************************************************************************************************************************
Chunk Filter

Split data up into chunks so it can be written (and later read) without knowing the eventual size of the data.
***********************************************************************************************************************************/
#ifndef COMMON_IO_FILTER_CHUNK_H
#define COMMON_IO_FILTER_CHUNK_H

#include "common/io/filter/filter.h"

/***********************************************************************************************************************************
Filter type constant
***********************************************************************************************************************************/
#define CHUNK_FILTER_TYPE                                           STRID5("chunk", 0xb755030)

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
FV_EXTERN IoFilter *ioChunkNew(void);

#endif
