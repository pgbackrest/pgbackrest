/***********************************************************************************************************************************
Storage Helper Test Harness

Helper functions for testing the storage helper.
***********************************************************************************************************************************/
#ifndef TEST_COMMON_HARNESS_STORAGE_HELPER_H
#define TEST_COMMON_HARNESS_STORAGE_HELPER_H

#include "storage/helper.h"

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Free all storage helper objects. This should be done on any config load to ensure that stanza changes are honored.
void hrnStorageHelperFree(void);

#endif
