/***********************************************************************************************************************************
Harness for Generating Test Info Files
***********************************************************************************************************************************/
#include "common/type/buffer.h"
#include "info/info.h"

#include "common/harnessStorage.h"

/***********************************************************************************************************************************
Write info to a file and add the checksum
***********************************************************************************************************************************/
#define HRN_INFO_PUT(storage, file, info, ...)                                                                                     \
    do                                                                                                                             \
    {                                                                                                                              \
        hrnTestLogPrefix(__LINE__);                                                                                                \
        hrnStoragePut(                                                                                                             \
            storage, file, harnessInfoChecksumZ(info), "put info", (HrnStoragePutParam){VAR_PARAM_INIT, __VA_ARGS__});             \
    }                                                                                                                              \
    while (0)

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
Buffer *harnessInfoChecksum(const String *info);
Buffer *harnessInfoChecksumZ(const char *info);

void harnessInfoLoadNewCallback(void *callbackData, const String *section, const String *key, const Variant *value);
