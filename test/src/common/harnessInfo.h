/***********************************************************************************************************************************
Harness for Generating Test Info Files
***********************************************************************************************************************************/
#include "common/type/buffer.h"
#include "info/info.h"

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
Buffer *harnessInfoChecksum(const String *info);
Buffer *harnessInfoChecksumZ(const char *info);

void harnessInfoLoadNewCallback(
    InfoCallbackType type, void *callbackData, const String *section, const String *key, const String *value);
