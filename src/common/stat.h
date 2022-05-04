/***********************************************************************************************************************************
Statistics Collector

Collect simple statistics that can be output to a KeyValue for processing and logging. Each stat has a String that identifies it
uniquely and will also be used in the output. Individual stats do not need to be created in advance since they will be created as
needed at runtime. However, statInit() must be called before any other stat*() functions.

NOTE: Statistics are held in a sorted list so there is some cost involved in each lookup. In general, statistics should be used for
relatively important or high-latency operations where measurements are critical. For instance, using statistics to count the
iterations of a loop would likely be a bad idea.
***********************************************************************************************************************************/
#ifndef COMMON_STAT_H
#define COMMON_STAT_H

#include "common/type/string.h"

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Initialize the stats collector
void statInit(void);

// Increment stat by one
void statInc(const String *key);

// Output stats to JSON
String *statToJson(void);

#endif
