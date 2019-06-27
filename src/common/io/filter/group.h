/***********************************************************************************************************************************
IO Filter Group

Process data through an arbitrary group of filters in the order added by the user using ioFilterGroupAdd().  After processing
results can be gathered using ioFilterGroupResult() for any filters that produce results.

Processing is complex and asymmetric for read/write so should be done via the IoRead and IoWrite objects.  General users need
only call ioFilterGroupNew(), ioFilterGroupAdd(), and ioFilterGroupResult().
***********************************************************************************************************************************/
#ifndef COMMON_IO_FILTER_GROUP_H
#define COMMON_IO_FILTER_GROUP_H

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
#define IO_FILTER_GROUP_TYPE                                        IoFilterGroup
#define IO_FILTER_GROUP_PREFIX                                      ioFilterGroup

typedef struct IoFilterGroup IoFilterGroup;

#include "common/io/filter/filter.h"
#include "common/type/string.h"

/***********************************************************************************************************************************
Constructor
***********************************************************************************************************************************/
IoFilterGroup *ioFilterGroupNew(void);

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
IoFilterGroup *ioFilterGroupAdd(IoFilterGroup *this, IoFilter *filter);
void ioFilterGroupOpen(IoFilterGroup *this);
void ioFilterGroupProcess(IoFilterGroup *this, const Buffer *input, Buffer *output);
void ioFilterGroupClose(IoFilterGroup *this);

/***********************************************************************************************************************************
Getters
***********************************************************************************************************************************/
bool ioFilterGroupDone(const IoFilterGroup *this);
bool ioFilterGroupInputSame(const IoFilterGroup *this);
Variant *ioFilterGroupParamAll(const IoFilterGroup *this);
const Variant *ioFilterGroupResult(const IoFilterGroup *this, const String *filterType);
const Variant *ioFilterGroupResultAll(const IoFilterGroup *this);
unsigned int ioFilterGroupSize(const IoFilterGroup *this);

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
void ioFilterGroupFree(IoFilterGroup *this);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
String *ioFilterGroupToLog(const IoFilterGroup *this);

#define FUNCTION_LOG_IO_FILTER_GROUP_TYPE                                                                                          \
    IoFilterGroup *
#define FUNCTION_LOG_IO_FILTER_GROUP_FORMAT(value, buffer, bufferSize)                                                             \
    FUNCTION_LOG_STRING_OBJECT_FORMAT(value, ioFilterGroupToLog, buffer, bufferSize)

#endif
