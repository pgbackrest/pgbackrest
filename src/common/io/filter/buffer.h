/***********************************************************************************************************************************
IO Buffer Filter

Move data from the input buffer to the output buffer without overflowing the output buffer.  Automatically used as the last filter
in a FilterGroup if the last filter is not already an InOut filter, so there is no reason to add it manually to a FilterGroup.
***********************************************************************************************************************************/
#ifndef COMMON_IO_FILTER_BUFFER_H
#define COMMON_IO_FILTER_BUFFER_H

#include "common/io/filter/filter.h"
#include "common/type/buffer.h"

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
IoFilter *ioBufferNew(void);

#endif
