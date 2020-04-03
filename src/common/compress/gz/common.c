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
        const ErrorType *errorType = &FormatError;

        switch (error)
        {
            // Not exactly an error, but since we are not using custom dictionaries it shouldn't be possible to get this result
            case Z_NEED_DICT:
            {
                errorMsg = "need dictionary";
                errorType = &AssertError;
                break;
            }

            // We should not get this error -- included for completeness
            case Z_ERRNO:
            {
                errorMsg = "file error";
                errorType = &AssertError;
                break;
            }

            case Z_STREAM_ERROR:
            {
                errorMsg = "stream error";
                break;
            }

            case Z_DATA_ERROR:
            {
                errorMsg = "data error";
                break;
            }

            case Z_MEM_ERROR:
            {
                errorMsg = "insufficient memory";
                errorType = &MemoryError;
                break;
            }

            // This error indicates an error in the code -- there should always be space in the buffer
            case Z_BUF_ERROR:
            {
                errorMsg = "no space in buffer";
                errorType = &AssertError;
                break;
            }

            case Z_VERSION_ERROR:
            {
                errorMsg = "incompatible version";
                break;
            }

            default:
            {
                errorMsg = "unknown error";
                errorType = &AssertError;
            }
        }

        THROWP_FMT(errorType, "zlib threw error: [%d] %s", error, errorMsg);
    }

    return error;
}
