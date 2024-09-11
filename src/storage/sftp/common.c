/***********************************************************************************************************************************
Sftp Common
***********************************************************************************************************************************/
#include "build.auto.h"

#ifdef HAVE_LIBSSH2

#include <libssh2_sftp.h>

#include "storage/sftp/common.h"
#include "common/debug.h"

/**********************************************************************************************************************************/
FN_EXTERN const char *
libssh2SftpErrorMsg(const uint64_t error)
{
    const char *errorMsg;

    switch (error)
    {
        /* SFTP Error Status Codes (returned by libssh2_sftp_last_error() ) */
        case LIBSSH2_FX_EOF:
            errorMsg = "LIBSSH2_FX_EOF";
            break;
        case LIBSSH2_FX_NO_SUCH_FILE:
            errorMsg = "LIBSSH2_FX_NO_SUCH_FILE";
            break;
        case LIBSSH2_FX_PERMISSION_DENIED:
            errorMsg = "LIBSSH2_FX_PERMISSION_DENIED";
            break;
        case LIBSSH2_FX_FAILURE:
            errorMsg = "LIBSSH2_FX_FAILURE";
            break;
        case LIBSSH2_FX_BAD_MESSAGE:
            errorMsg = "LIBSSH2_FX_BAD_MESSAGE";
            break;
        case LIBSSH2_FX_NO_CONNECTION:
            errorMsg = "LIBSSH2_FX_NO_CONNECTION";
            break;
        case LIBSSH2_FX_CONNECTION_LOST:
            errorMsg = "LIBSSH2_FX_CONNECTION_LOST";
            break;
        case LIBSSH2_FX_OP_UNSUPPORTED:
            errorMsg = "LIBSSH2_FX_OP_UNSUPPORTED";
            break;
        case LIBSSH2_FX_INVALID_HANDLE:
            errorMsg = "LIBSSH2_FX_INVALID_HANDLE";
            break;
        case LIBSSH2_FX_NO_SUCH_PATH:
            errorMsg = "LIBSSH2_FX_NO_SUCH_PATH";
            break;
        case LIBSSH2_FX_FILE_ALREADY_EXISTS:
            errorMsg = "LIBSSH2_FX_FILE_ALREADY_EXISTS";
            break;
        case LIBSSH2_FX_WRITE_PROTECT:
            errorMsg = "LIBSSH2_FX_WRITE_PROTECT";
            break;
        case LIBSSH2_FX_NO_MEDIA:
            errorMsg = "LIBSSH2_FX_NO_MEDIA";
            break;
        case LIBSSH2_FX_NO_SPACE_ON_FILESYSTEM:
            errorMsg = "LIBSSH2_FX_NO_SPACE_ON_FILESYSTEM";
            break;
        case LIBSSH2_FX_QUOTA_EXCEEDED:
            errorMsg = "LIBSSH2_FX_QUOTA_EXCEEDED";
            break;
        case LIBSSH2_FX_UNKNOWN_PRINCIPAL:
            errorMsg = "LIBSSH2_FX_UNKNOWN_PRINCIPAL";
            break;
        case LIBSSH2_FX_LOCK_CONFLICT:
            errorMsg = "LIBSSH2_FX_LOCK_CONFLICT";
            break;
        case LIBSSH2_FX_DIR_NOT_EMPTY:
            errorMsg = "LIBSSH2_FX_DIR_NOT_EMPTY";
            break;
        case LIBSSH2_FX_NOT_A_DIRECTORY:
            errorMsg = "LIBSSH2_FX_NOT_A_DIRECTORY";
            break;
        case LIBSSH2_FX_INVALID_FILENAME:
            errorMsg = "LIBSSH2_FX_INVALID_FILENAME";
            break;
        case LIBSSH2_FX_LINK_LOOP:
            errorMsg = "LIBSSH2_FX_LINK_LOOP";
            break;
        default:
            errorMsg = "unknown error";
            break;
    }

    return errorMsg;
}

#endif // HAVE_LIBSSH2
