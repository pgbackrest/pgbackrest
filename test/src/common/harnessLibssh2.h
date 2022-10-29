/***********************************************************************************************************************************
libssh2 Test Harness

Scripted testing for libssh2 so exact results can be returned for unit testing.  See sftp unit tests for usage examples.
***********************************************************************************************************************************/
#ifndef TEST_COMMON_HARNESS_LIBSSH2_H
#define TEST_COMMON_HARNESS_LIBSSH2_H

//#ifndef HARNESS_LIBSSH2_REAL

#include <libssh2.h>
#include <libssh2_sftp.h>
#include <stdbool.h>

#include "common/macro.h"
#include "common/time.h"
#include "version.h"

/***********************************************************************************************************************************
Function constants
***********************************************************************************************************************************/
#define HRNLIBSSH2_EXIT                                             "libssh2_exit"
#define HRNLIBSSH2_HOSTKEY_HASH                                     "libssh2_hostkey_hash"
#define HRNLIBSSH2_INIT                                             "libssh2_init"
#define HRNLIBSSH2_SESSION_DISCONNECT_EX                            "libssh2_session_disconnect_ex"
#define HRNLIBSSH2_SESSION_FREE                                     "libssh2_session_free"
#define HRNLIBSSH2_SESSION_HANDSHAKE                                "libssh2_session_handshake"
#define HRNLIBSSH2_SESSION_INIT_EX                                  "libssh2_session_init_ex"
#define HRNLIBSSH2_SESSION_LAST_ERRNO                               "libssh2_session_last_errno"
#define HRNLIBSSH2_SESSION_LAST_ERROR                               "libssh2_session_last_error"
#define HRNLIBSSH2_SESSION_SET_BLOCKING                             "libssh2_session_set_blocking"
#define HRNLIBSSH2_SFTP_CLOSE_HANDLE                                "libssh2_sftp_close_handle"
#define HRNLIBSSH2_SFTP_FSTAT_EX                                    "libssh2_sftp_fstat_ex"
#define HRNLIBSSH2_SFTP_FSYNC                                       "libssh2_sftp_fsync"
#define HRNLIBSSH2_SFTP_INIT                                        "libssh2_sftp_init"
#define HRNLIBSSH2_SFTP_LAST_ERROR                                  "libssh2_sftp_last_error"
#define HRNLIBSSH2_SFTP_MKDIR_EX                                    "libssh2_sftp_mkdir_ex"
#define HRNLIBSSH2_SFTP_OPEN_EX                                     "libssh2_sftp_open_ex"
#define HRNLIBSSH2_SFTP_READ                                        "libssh2_sftp_read"
#define HRNLIBSSH2_SFTP_READDIR_EX                                  "libssh2_sftp_readdir_ex"
#define HRNLIBSSH2_SFTP_RENAME_EX                                   "libssh2_sftp_rename_ex"
#define HRNLIBSSH2_SFTP_RMDIR_EX                                    "libssh2_sftp_rmdir_ex"
#define HRNLIBSSH2_SFTP_SEEK64                                      "libssh2_sftp_seek64"
#define HRNLIBSSH2_SFTP_SHUTDOWN                                    "libssh2_sftp_shutdown"
#define HRNLIBSSH2_SFTP_STAT_EX                                     "libssh2_sftp_stat_ex"
#define HRNLIBSSH2_SFTP_SYMLINK_EX                                  "libssh2_sftp_symlink_ex"
#define HRNLIBSSH2_SFTP_UNLINK_EX                                   "libssh2_sftp_unlink_ex"
#define HRNLIBSSH2_SFTP_WRITE                                       "libssh2_sftp_write"
#define HRNLIBSSH2_USERAUTH_PUBLICKEY_FROMFILE_EX                   "libssh2_userauth_publickey_fromfile_ex"

/***********************************************************************************************************************************
Structure for scripting libssh2 responses
***********************************************************************************************************************************/
typedef struct HarnessLibssh2
{
    unsigned int session;                                           // Session number when multiple sessions are run concurrently
    const char *function;                                           // Function call expected
    const char *param;                                              // Params expected by the function for verification
    int resultInt;                                                  // Int result value
    uint64_t resultUInt;                                            // UInt result value
    const char *resultZ;                                            // Zero-terminated result value
    bool resultNull;                                                // Return null from function that normally returns a struct ptr
    uint64_t attrPerms;                                             // libssh2 attr perms
    TimeMSec sleep;                                                 // Sleep specified milliseconds before returning from function
} HarnessLibssh2;

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
void harnessLibssh2ScriptSet(HarnessLibssh2 *harnessLibssh2ScriptParam);

//#endif // HARNESS_LIBSSH2_REAL

#endif
