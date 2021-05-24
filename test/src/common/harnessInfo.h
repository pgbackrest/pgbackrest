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
    TEST_RESULT_VOID(                                                                                                              \
        hrnStoragePut(storage, file, harnessInfoChecksumZ(info), (HrnStoragePutParam){VAR_PARAM_INIT, __VA_ARGS__}),               \
        strZ(strNewFmt("put info %s", hrnStoragePutLog(storage, file, LF_BUF, (HrnStoragePutParam){VAR_PARAM_INIT, __VA_ARGS__}))))

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
Buffer *harnessInfoChecksum(const String *info);
Buffer *harnessInfoChecksumZ(const char *info);

void harnessInfoLoadNewCallback(void *callbackData, const String *section, const String *key, const Variant *value);
