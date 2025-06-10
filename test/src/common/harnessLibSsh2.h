/***********************************************************************************************************************************
libssh2 Test Harness

Scripted testing for libssh2 so exact results can be returned for unit testing. See sftp unit tests for usage examples.
***********************************************************************************************************************************/
#ifndef TEST_COMMON_HARNESS_LIBSSH2_H
#define TEST_COMMON_HARNESS_LIBSSH2_H

#ifdef HAVE_LIBSSH2

#ifndef HARNESS_LIBSSH2_REAL

#include <libssh2.h>
#include <libssh2_sftp.h>
#include <stdbool.h>

#include "common/macro.h"
#include "common/time.h"
#include "version.h"

/***********************************************************************************************************************************
libssh2 authorization constants
***********************************************************************************************************************************/
#define KEYPRIV                                                     STRDEF("/home/" TEST_USER "/.ssh/id_rsa")
#define KEYPUB                                                      STRDEF("/home/" TEST_USER "/.ssh/id_rsa.pub")
#define KEYPRIV_CSTR                                                "/home/" TEST_USER "/.ssh/id_rsa"
#define KEYPUB_CSTR                                                 "/home/" TEST_USER "/.ssh/id_rsa.pub"
#define KNOWNHOSTS_FILE_CSTR                                        "/home/" TEST_USER "/.ssh/known_hosts"
#define KNOWNHOSTS2_FILE_CSTR                                       "/home/" TEST_USER "/.ssh/known_hosts2"
#define ETC_KNOWNHOSTS_FILE_CSTR                                    "/etc/ssh/ssh_known_hosts"
#define ETC_KNOWNHOSTS2_FILE_CSTR                                   "/etc/ssh/ssh_known_hosts2"
#define HOSTKEY                                                     "12345678901234567890"

/***********************************************************************************************************************************
Function constants
***********************************************************************************************************************************/
#define HRNLIBSSH2_HOSTKEY_HASH                                     "libssh2_hostkey_hash"
#define HRNLIBSSH2_INIT                                             "libssh2_init"
#define HRNLIBSSH2_KNOWNHOST_ADDC                                   "libssh2_knownhost_addc"
#define HRNLIBSSH2_KNOWNHOST_CHECKP                                 "libssh2_knownhost_checkp"
#define HRNLIBSSH2_KNOWNHOST_FREE                                   "libssh2_knownhost_free"
#define HRNLIBSSH2_KNOWNHOST_INIT                                   "libssh2_knownhost_init"
#define HRNLIBSSH2_KNOWNHOST_READFILE                               "libssh2_knownhost_readfile"
#define HRNLIBSSH2_KNOWNHOST_WRITEFILE                              "libssh2_knownhost_writefile"
#define HRNLIBSSH2_SESSION_BLOCK_DIRECTIONS                         "libssh2_session_block_directions"
#define HRNLIBSSH2_SESSION_DISCONNECT_EX                            "libssh2_session_disconnect_ex"
#define HRNLIBSSH2_SESSION_FREE                                     "libssh2_session_free"
#define HRNLIBSSH2_SESSION_HANDSHAKE                                "libssh2_session_handshake"
#define HRNLIBSSH2_SESSION_HOSTKEY                                  "libssh2_session_hostkey"
#define HRNLIBSSH2_SESSION_INIT_EX                                  "libssh2_session_init_ex"
#define HRNLIBSSH2_SESSION_LAST_ERRNO                               "libssh2_session_last_errno"
#define HRNLIBSSH2_SESSION_LAST_ERROR                               "libssh2_session_last_error"
#define HRNLIBSSH2_SFTP_CLOSE_HANDLE                                "libssh2_sftp_close_handle"
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
Macros for defining groups of functions that implement commands
***********************************************************************************************************************************/
// Set of functions mimicking libssh2 initialization and authorization
#define HRNLIBSSH2_MACRO_STARTUP()                                                                                                 \
    {.function = HRNLIBSSH2_INIT, .param = "[0]", .resultInt = 0},                                                                 \
    {.function = HRNLIBSSH2_SESSION_INIT_EX, .param = "[null,null,null,null]"},                                                    \
    {.function = HRNLIBSSH2_SESSION_HANDSHAKE, .param = HANDSHAKE_PARAM, .resultInt = 0},                                          \
    {.function = HRNLIBSSH2_KNOWNHOST_INIT},                                                                                       \
    {.function = HRNLIBSSH2_KNOWNHOST_READFILE, .param = "[\"" KNOWNHOSTS_FILE_CSTR "\",1]", .resultInt = 5},                      \
    {.function = HRNLIBSSH2_SESSION_HOSTKEY, .len = 20, .type = LIBSSH2_HOSTKEY_TYPE_RSA, .resultZ = HOSTKEY},                     \
    {.function = HRNLIBSSH2_KNOWNHOST_CHECKP, .param = "[\"localhost\",22,\"" HOSTKEY "\",20,65537]",                              \
     .resultInt = LIBSSH2_KNOWNHOST_CHECK_MATCH},                                                                                  \
    {.function = HRNLIBSSH2_USERAUTH_PUBLICKEY_FROMFILE_EX,                                                                        \
    .param = "[\"" TEST_USER "\"," TEST_USER_LEN ",\"" KEYPUB_CSTR "\",\"" KEYPRIV_CSTR "\",null]",                                \
    .resultInt = 0},                                                                                                               \
    {.function = HRNLIBSSH2_SFTP_INIT}

// Set of functions mimicking libssh2 shutdown and disconnect
#define HRNLIBSSH2_MACRO_SHUTDOWN()                                                                                                \
    {.function = HRNLIBSSH2_SFTP_SHUTDOWN, .resultInt = 0},                                                                        \
    {.function = HRNLIBSSH2_SESSION_DISCONNECT_EX, .param ="[11,\"pgBackRest instance shutdown\",\"\"]", .resultInt = 0},          \
    {.function = HRNLIBSSH2_SESSION_FREE, .resultInt = 0},                                                                         \
    {.function = NULL}                                                                                                             \

// Older systems do not support LIBSSH2_HOSTKEY_HASH_SHA256
#ifdef LIBSSH2_HOSTKEY_HASH_SHA256
#define HOSTKEY_HASH_ENTRY()                                                                                                       \
    {.function = HRNLIBSSH2_HOSTKEY_HASH, .param = "[3]", .resultZ = "12345678910123456789"}
#else
#define HOSTKEY_HASH_ENTRY()                                                                                                       \
    {.function = HRNLIBSSH2_HOSTKEY_HASH, .param = "[2]", .resultZ = "12345678910123456789"}
#endif

/***********************************************************************************************************************************
Structure for scripting libssh2 responses
***********************************************************************************************************************************/
typedef struct HrnLibSsh2
{
    unsigned int session;                                           // Session number when multiple sessions are run concurrently
    const char *function;                                           // Function call expected
    const char *param;                                              // Params expected by the function for verification
    int resultInt;                                                  // Int result value
    uint64_t resultUInt;                                            // UInt result value
    const char *resultZ;                                            // Zero-terminated result value
    bool resultNull;                                                // Return null from function that normally returns a struct ptr
    uint64_t flags;                                                 // libssh2 flags
    uint64_t attrPerms;                                             // libssh2 attr perms
    uint64_t atime, mtime;                                          // libssh2 timestamps
    uint64_t uid, gid;                                              // libssh2 uid/gid
    uint64_t filesize;                                              // libssh2 filesize
    uint64_t offset;                                                // libssh2 seek offset
    const String *symlinkExTarget;                                  // libssh2_sftp_symlink_ex target
    const String *fileName;                                         // libssh2_readdir* libssh2_stat* filename
    const String *readBuffer;                                       // what to copy into read buffer
    TimeMSec sleep;                                                 // Sleep specified milliseconds before returning from function
    size_t len;                                                     // libssh2_session_hostkey len
    int type;                                                       // libssh2_session_hostkey type
    const char *errMsg;                                             // libssh2_session_last_error error msg
} HrnLibSsh2;

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
void hrnLibSsh2ScriptSet(HrnLibSsh2 *hrnLibSsh2ScriptParam);

#endif // HARNESS_LIBSSH2_REAL

#endif // HAVE_LIBSSH2

#endif // TEST_COMMON_HARNESS_LIBSSH2_H
