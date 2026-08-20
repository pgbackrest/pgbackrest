/***********************************************************************************************************************************
Harness for Generating Test Info Files
***********************************************************************************************************************************/
#include "common/type/buffer.h"
#include "info/info.h"

#include "harness/storage.h"

/***********************************************************************************************************************************
Format that new repositories are created with, i.e. the default of the repo-format option in build/config.yaml. Only tests need to
know this since the option supplies it everywhere else.
***********************************************************************************************************************************/
#define REPOSITORY_FORMAT_DEFAULT                                   REPOSITORY_FORMAT_5

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
Buffer *harnessInfoChecksumFormat(unsigned int format, const String *info);
Buffer *harnessInfoChecksumZ(const char *info);

void harnessInfoLoadNewCallback(void *callbackData, const String *section, const String *key, JsonRead *json);
