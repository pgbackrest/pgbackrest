/***********************************************************************************************************************************
Gz Common
***********************************************************************************************************************************/
#include "build.auto.h"

#include <zlib.h>

#include "common/compress/gz/common.h"
#include "common/debug.h"
#include "common/memContext.h"

/**********************************************************************************************************************************/
int
gzError(int error)
{
    if (error != Z_OK && error != Z_STREAM_END)
    {
        const char *errorMsg;
        const ErrorType *errorType = &AssertError;

        switch (error)
        {
            // Not exactly an error, but since we are not using custom dictionaries it shouldn't be possible to get this result
            case Z_NEED_DICT:
                errorMsg = "need dictionary";
                break;

            // We should not get this error -- included for completeness
            case Z_ERRNO:
                errorMsg = "file error";
                break;

            case Z_STREAM_ERROR:
                errorMsg = "stream error";
                errorType = &FormatError;
                break;

            case Z_DATA_ERROR:
                errorMsg = "data error";
                errorType = &FormatError;
                break;

            case Z_MEM_ERROR:
                errorMsg = "insufficient memory";
                errorType = &MemoryError;
                break;

            // This error indicates an error in the code -- there should always be space in the buffer
            case Z_BUF_ERROR:
                errorMsg = "no space in buffer";
                break;

            case Z_VERSION_ERROR:
                errorMsg = "incompatible version";
                errorType = &FormatError;
                break;

            default:
                errorMsg = "unknown error";
                break;
        }

        THROWP_FMT(errorType, "zlib threw error: [%d] %s", error, errorMsg);
    }

    return error;
}
