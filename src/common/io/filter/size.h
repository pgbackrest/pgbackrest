/***********************************************************************************************************************************
IO Size Filter

Count all bytes that pass through the filter.  Useful for getting file/IO size if added first in a FilterGroup with IoRead or last
in a FilterGroup with IoWrite.
***********************************************************************************************************************************/
#ifndef COMMON_IO_FILTER_SIZE_H
#define COMMON_IO_FILTER_SIZE_H

#include "common/io/filter/filter.h"

/***********************************************************************************************************************************
Constructor
***********************************************************************************************************************************/
IoFilter *ioSizeNew(void);

#endif
