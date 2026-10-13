/***********************************************************************************************************************************
Fuzz Target Driver
***********************************************************************************************************************************/
#ifndef TEST_FUZZ_FUZZ_H
#define TEST_FUZZ_FUZZ_H

#include "common/type/buffer.h"

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Process an input from the fuzzing engine. Each target defines this function. Errors thrown for invalid input are discarded but a
// fatal error aborts.
void fuzzTarget(const Buffer *input);

#endif
